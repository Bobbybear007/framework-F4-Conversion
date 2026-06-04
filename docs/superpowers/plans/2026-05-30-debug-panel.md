# In-Game Debug Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an in-game debugger panel accessible via a draggable overlay button that appears in the pause menu — showing a real-time log viewer, JS console, and active view inspector.

**Architecture:** A `DebugLogSink` captures spdlog output into a ring buffer and pushes lines to the panel only when it's open. `DebugManager` owns both Ultralight views, listens for `MenuOpenCloseEvent` on `"PauseMenu"`, and wires all JS↔C++ communication. The button is draggable HTML; position is persisted to `Data/PrismaUI_F4/debug_button_pos.json`.

**Tech Stack:** CommonLibF4 (RE::BSTEventSink, RE::MenuOpenCloseEvent, RE::UI), spdlog custom sink, PrismaUI API (IVPrismaUI3), Win32 filesystem, HTML/CSS/JS (Ultralight).

**Note:** No automated test framework — verification is build success + in-game log inspection.

---

## File Map

| Action | Path | Responsibility |
|---|---|---|
| Create | `src/Debug/DebugLogSink.h` | Header-only spdlog sink: ring buffer + conditional push |
| Create | `src/Debug/DebugManager.h` | Class declaration: BSTEventSink, public API |
| Create | `src/Debug/DebugManager.cpp` | Full implementation: Init, events, view lifecycle, JS handlers |
| Create | `assets/views/debug_button.html` | Draggable "≡ Debug" button |
| Create | `assets/views/debug_panel.html` | 3-tab debug panel |
| Modify | `src/PrismaUI/ViewManager.h` | Add `GetViewsSnapshot()` declaration |
| Modify | `src/PrismaUI/ViewManager.cpp` | Add `GetViewsSnapshot()` implementation |
| Modify | `src/main.cpp` | Call `DebugManager::Init` on kGameDataReady, `OnPostLoadGame` on kPostLoadGame/kNewGame |

---

### Task 1: Create `DebugLogSink.h`

**Files:**
- Create: `src/Debug/DebugLogSink.h`

- [ ] **Step 1: Create the directory and file**

```cpp
// src/Debug/DebugLogSink.h
#pragma once

#include <spdlog/sinks/base_sink.h>
#include <chrono>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace PrismaUI::Debug {

    using LogPushCallback = std::function<void(const std::string& jsonLine)>;

    // Header-only custom spdlog sink.
    // - Always appends to a ring buffer (kMaxLines cap).
    // - Only calls pushCallback_ when one is set (panel is open).
    // Performance: when no callback is set, cost is one deque push + size check per log line.
    class DebugLogSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        static constexpr size_t kMaxLines = 1000;

        // Set callback invoked for each new line (call from OpenPanel).
        void SetPushCallback(LogPushCallback cb) {
            std::lock_guard lock(mutex_);
            pushCallback_ = std::move(cb);
        }

        // Clear callback (call from ClosePanel).
        void ClearPushCallback() {
            std::lock_guard lock(mutex_);
            pushCallback_ = nullptr;
        }

        // Returns all buffered lines as a JSON array string — call once on panel open.
        std::string GetAllLogs() {
            std::lock_guard lock(mutex_);
            std::string result = "[";
            bool first = true;
            for (const auto& line : buffer_) {
                if (!first) result += ',';
                first = false;
                result += line;
            }
            result += ']';
            return result;
        }

        void ClearBuffer() {
            std::lock_guard lock(mutex_);
            buffer_.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            // base_sink<std::mutex> holds mutex_ before calling this.

            // Determine level string
            const char* level = "info";
            switch (msg.level) {
                case spdlog::level::trace:
                case spdlog::level::debug:    level = "debug";    break;
                case spdlog::level::info:     level = "info";     break;
                case spdlog::level::warn:     level = "warn";     break;
                case spdlog::level::err:      level = "error";    break;
                case spdlog::level::critical: level = "critical"; break;
                default: break;
            }

            // Format timestamp HH:MM:SS.mmm
            auto tse = msg.time.time_since_epoch();
            auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(tse).count() % 1000;
            auto sec = std::chrono::duration_cast<std::chrono::seconds>(tse).count();
            std::time_t tt = static_cast<std::time_t>(sec);
            std::tm tm_info{};
            localtime_s(&tm_info, &tt);
            char tsBuf[16];
            snprintf(tsBuf, sizeof(tsBuf), "%02d:%02d:%02d.%03lld",
                     tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                     static_cast<long long>(ms));

            // Get message text
            std::string text(msg.payload.begin(), msg.payload.end());

            // JSON-escape the message text
            std::string escaped;
            escaped.reserve(text.size() + 8);
            for (char c : text) {
                switch (c) {
                    case '"':  escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n";  break;
                    case '\r': escaped += "\\r";  break;
                    default:   escaped += c;      break;
                }
            }

            std::string jsonLine =
                std::string("{\"level\":\"") + level +
                "\",\"msg\":\"" + escaped +
                "\",\"ts\":\"" + tsBuf + "\"}";

            // Ring buffer
            buffer_.push_back(jsonLine);
            if (buffer_.size() > kMaxLines)
                buffer_.pop_front();

            // Push to panel if open
            if (pushCallback_)
                pushCallback_(jsonLine);
        }

        void flush_() override {}

    private:
        std::deque<std::string> buffer_;
        LogPushCallback pushCallback_;
        // Note: mutex_ is inherited from base_sink<std::mutex> (protected member).
        // All methods above acquire it via std::lock_guard lock(mutex_) or
        // are called with it already held (sink_it_, flush_).
    };

}  // namespace PrismaUI::Debug
```

- [ ] **Step 2: Commit**

```bash
git add "src/Debug/DebugLogSink.h"
git commit -m "feat: add DebugLogSink - spdlog ring buffer sink"
```

---

### Task 2: Add `GetViewsSnapshot()` to ViewManager

**Files:**
- Modify: `src/PrismaUI/ViewManager.h`
- Modify: `src/PrismaUI/ViewManager.cpp`

- [ ] **Step 1: Add declaration to `ViewManager.h`**

In `src/PrismaUI/ViewManager.h`, find the existing function declarations and add:

```cpp
    // Returns a JSON array string describing all active views.
    // Format: [{"id":"<id>","url":"<url>","visible":<bool>,"focused":<bool>,"order":<int>}, ...]
    std::string GetViewsSnapshot();
```

- [ ] **Step 2: Add implementation to `ViewManager.cpp`**

At the end of `namespace PrismaUI::ViewManager` in `ViewManager.cpp`, before the closing `}`, add:

```cpp
    std::string GetViewsSnapshot() {
        Core::PrismaViewId focusedId = InputHandler::GetFocusedViewId();

        std::shared_lock lock(viewsMutex);
        std::string json = "[";
        bool first = true;
        for (const auto& [id, view] : views) {
            if (!view) continue;
            if (!first) json += ',';
            first = false;

            const bool visible = !view->isHidden.load();
            const bool focused = (id == focusedId);

            // JSON-escape the URL
            std::string escapedUrl;
            for (char c : view->lastLoadedUrl) {
                if (c == '"')  escapedUrl += "\\\"";
                else if (c == '\\') escapedUrl += "\\\\";
                else escapedUrl += c;
            }

            json += "{\"id\":\"" + std::to_string(id) + "\"";
            json += ",\"url\":\"" + escapedUrl + "\"";
            json += ",\"visible\":" + std::string(visible ? "true" : "false");
            json += ",\"focused\":" + std::string(focused ? "true" : "false");
            json += ",\"order\":"  + std::to_string(view->order) + "}";
        }
        json += "]";
        return json;
    }
```

- [ ] **Step 3: Build to verify no compile errors**

```powershell
cd "E:\F4SE OG\Prisma\PrismaUI_F4 New Gen"
.\build.bat 2>&1 | Select-String -Pattern "error|warning" | Select-Object -First 20
```

Expected: no new errors.

- [ ] **Step 4: Commit**

```bash
git add "src/PrismaUI/ViewManager.h" "src/PrismaUI/ViewManager.cpp"
git commit -m "feat: add ViewManager::GetViewsSnapshot for debug panel"
```

---

### Task 3: Create `DebugManager.h`

**Files:**
- Create: `src/Debug/DebugManager.h`

- [ ] **Step 1: Write the header**

```cpp
// src/Debug/DebugManager.h
#pragma once

#include "DebugLogSink.h"
#include "PrismaUI_F4_API.h"

#include <RE/Fallout.h>
#include <filesystem>
#include <memory>
#include <string>

namespace PrismaUI::Debug {

    class DebugManager : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static DebugManager& GetSingleton() noexcept {
            static DebugManager instance;
            return instance;
        }

        // Call on kGameDataReady: installs log sink, registers MenuOpenCloseEvent.
        void Init(PRISMA_UI_API::IVPrismaUI3* api) noexcept;

        // Call on kPostLoadGame and kNewGame: creates/recreates both views.
        void OnPostLoadGame() noexcept;

        // BSTEventSink implementation
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent& event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* source) override;

    private:
        DebugManager() = default;
        ~DebugManager() = default;
        DebugManager(const DebugManager&) = delete;
        DebugManager& operator=(const DebugManager&) = delete;

        void OpenPanel() noexcept;
        void ClosePanel() noexcept;
        void SaveButtonPos(const std::string& json) noexcept;
        void SendButtonPos() noexcept;  // Reads saved pos and InteropCalls "setPos" to button

        // JS listener handlers
        void OnDebugToggle(const std::string&) noexcept;
        void OnSavePos(const std::string& json) noexcept;
        void OnConsoleRun(const std::string& json) noexcept;
        void OnViewsRefresh(const std::string&) noexcept;
        void OnViewAction(const std::string& json) noexcept;

        PRISMA_UI_API::IVPrismaUI3* api_    = nullptr;
        PrismaView debugButtonView_          = 0;
        PrismaView debugPanelView_           = 0;
        bool panelOpen_                      = false;
        std::shared_ptr<DebugLogSink> sink_;
        std::filesystem::path buttonPosPath_;
    };

}  // namespace PrismaUI::Debug
```

- [ ] **Step 2: Commit**

```bash
git add "src/Debug/DebugManager.h"
git commit -m "feat: add DebugManager header"
```

---

### Task 4: Create `DebugManager.cpp` — Init, event handler, view lifecycle

**Files:**
- Create: `src/Debug/DebugManager.cpp`

- [ ] **Step 1: Write the file**

```cpp
// src/Debug/DebugManager.cpp
#include "DebugManager.h"

#include <fstream>
#include <sstream>

#include "PrismaUI/ViewManager.h"

namespace PrismaUI::Debug {

    // ──────────────────────────────────────────────────────────────
    // Init
    // ──────────────────────────────────────────────────────────────

    void DebugManager::Init(PRISMA_UI_API::IVPrismaUI3* api) noexcept {
        api_ = api;

        // Install log sink
        sink_ = std::make_shared<DebugLogSink>();
        spdlog::default_logger()->sinks().push_back(sink_);
        logger::info("[DebugManager] Log sink installed ({} lines buffered max)", DebugLogSink::kMaxLines);

        // Button position file path
        buttonPosPath_ = std::filesystem::current_path() / "Data" / "PrismaUI_F4" / "debug_button_pos.json";

        // Register for PauseMenu open/close events
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(this);
            logger::info("[DebugManager] Registered MenuOpenCloseEvent sink");
        } else {
            logger::error("[DebugManager] RE::UI::GetSingleton() returned null — MenuOpenCloseEvent not registered");
        }
    }

    // ──────────────────────────────────────────────────────────────
    // MenuOpenCloseEvent
    // ──────────────────────────────────────────────────────────────

    RE::BSEventNotifyControl DebugManager::ProcessEvent(
        const RE::MenuOpenCloseEvent& event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!api_) return RE::BSEventNotifyControl::kContinue;

        // Only care about PauseMenu
        if (event.menuName != RE::PauseMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        if (event.opening) {
            logger::debug("[DebugManager] PauseMenu opened — showing debug button");
            if (debugButtonView_ && api_->IsValid(debugButtonView_)) {
                api_->Show(debugButtonView_);
                SendButtonPos();
            } else {
                logger::warn("[DebugManager] ProcessEvent: debugButtonView_ is not valid");
            }
        } else {
            logger::debug("[DebugManager] PauseMenu closed — hiding debug button and panel");
            if (debugButtonView_ && api_->IsValid(debugButtonView_))
                api_->Hide(debugButtonView_);
            ClosePanel();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    // ──────────────────────────────────────────────────────────────
    // View lifecycle
    // ──────────────────────────────────────────────────────────────

    void DebugManager::OnPostLoadGame() noexcept {
        if (!api_) {
            logger::error("[DebugManager] OnPostLoadGame called but api_ is null");
            return;
        }

        // Recreate button view
        debugButtonView_ = api_->CreateView("debug_button.html",
            [this](PrismaView v) {
                // OnDomReady for button
                api_->RegisterJSListener(v, "prismaDebugToggle",
                    [this](const char* s) { OnDebugToggle(s ? s : ""); });
                api_->RegisterJSListener(v, "prismaDebugSavePos",
                    [this](const char* s) { OnSavePos(s ? s : ""); });
                logger::info("[DebugManager] debug_button.html DOM ready, listeners registered");
            });
        api_->Hide(debugButtonView_);

        // Recreate panel view
        debugPanelView_ = api_->CreateView("debug_panel.html",
            [this](PrismaView v) {
                // OnDomReady for panel
                api_->RegisterJSListener(v, "prismaDebugToggle",
                    [this](const char* s) { OnDebugToggle(s ? s : ""); });
                api_->RegisterJSListener(v, "prismaConsoleRun",
                    [this](const char* s) { OnConsoleRun(s ? s : ""); });
                api_->RegisterJSListener(v, "prismaViewsRefresh",
                    [this](const char* s) { OnViewsRefresh(s ? s : ""); });
                api_->RegisterJSListener(v, "prismaViewAction",
                    [this](const char* s) { OnViewAction(s ? s : ""); });
                logger::info("[DebugManager] debug_panel.html DOM ready, listeners registered");
            });
        api_->Hide(debugPanelView_);
        panelOpen_ = false;

        logger::info("[DebugManager] Views created: button={} panel={}", debugButtonView_, debugPanelView_);
    }

    // ──────────────────────────────────────────────────────────────
    // Panel open / close
    // ──────────────────────────────────────────────────────────────

    void DebugManager::OpenPanel() noexcept {
        if (!api_ || !debugPanelView_ || !api_->IsValid(debugPanelView_)) {
            logger::warn("[DebugManager] OpenPanel: panel view not valid");
            return;
        }
        if (panelOpen_) return;

        // Enable live log push before showing, so no lines are missed
        sink_->SetPushCallback([this](const std::string& jsonLine) {
            if (api_ && debugPanelView_ && api_->IsValid(debugPanelView_)) {
                api_->InteropCall(debugPanelView_, "appendLog", jsonLine.c_str());
            }
        });

        api_->Show(debugPanelView_);
        // pauseGame=false (PauseMenu already paused), disableFocusMenu=true (no extra overlay)
        api_->Focus(debugPanelView_, false, true);

        // Send buffered history
        std::string allLogs = sink_->GetAllLogs();
        api_->InteropCall(debugPanelView_, "bulkLog", allLogs.c_str());

        panelOpen_ = true;
        logger::info("[DebugManager] Panel opened");
    }

    void DebugManager::ClosePanel() noexcept {
        if (!panelOpen_) return;

        sink_->ClearPushCallback();

        if (api_ && debugPanelView_ && api_->IsValid(debugPanelView_)) {
            api_->Unfocus(debugPanelView_);
            api_->Hide(debugPanelView_);
        }

        panelOpen_ = false;
        logger::info("[DebugManager] Panel closed");
    }

    // ──────────────────────────────────────────────────────────────
    // Button position persistence
    // ──────────────────────────────────────────────────────────────

    void DebugManager::SaveButtonPos(const std::string& json) noexcept {
        try {
            std::ofstream f(buttonPosPath_);
            if (f.is_open()) {
                f << json;
                logger::debug("[DebugManager] Saved button position: {}", json);
            } else {
                logger::warn("[DebugManager] Failed to open button pos file for writing: {}", buttonPosPath_.string());
            }
        } catch (const std::exception& e) {
            logger::error("[DebugManager] SaveButtonPos exception: {}", e.what());
        }
    }

    void DebugManager::SendButtonPos() noexcept {
        if (!api_ || !debugButtonView_ || !api_->IsValid(debugButtonView_)) return;
        try {
            std::ifstream f(buttonPosPath_);
            if (f.is_open()) {
                std::ostringstream ss;
                ss << f.rdbuf();
                std::string json = ss.str();
                if (!json.empty()) {
                    api_->InteropCall(debugButtonView_, "setPos", json.c_str());
                    logger::debug("[DebugManager] Sent button position: {}", json);
                }
            }
            // If file doesn't exist, JS uses the CSS default (bottom-right)
        } catch (const std::exception& e) {
            logger::warn("[DebugManager] SendButtonPos exception: {}", e.what());
        }
    }

    // ──────────────────────────────────────────────────────────────
    // JS listener handlers
    // ──────────────────────────────────────────────────────────────

    void DebugManager::OnDebugToggle(const std::string&) noexcept {
        if (panelOpen_) ClosePanel(); else OpenPanel();
    }

    void DebugManager::OnSavePos(const std::string& json) noexcept {
        SaveButtonPos(json);
    }

    void DebugManager::OnConsoleRun(const std::string& json) noexcept {
        // Expect JSON: {"viewId":"<id>","script":"<js>"}
        // Simple manual parse — find "viewId" and "script" fields
        auto extractField = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = json.find(needle);
            if (pos == std::string::npos) return {};
            pos += needle.size();
            std::string value;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.size()) {
                    char next = json[pos + 1];
                    if (next == '"')  { value += '"';  pos += 2; }
                    else if (next == '\\') { value += '\\'; pos += 2; }
                    else if (next == 'n')  { value += '\n'; pos += 2; }
                    else if (next == 'r')  { value += '\r'; pos += 2; }
                    else { value += next; pos += 2; }
                } else {
                    value += json[pos++];
                }
            }
            return value;
        };

        std::string viewIdStr = extractField("viewId");
        std::string script    = extractField("script");

        if (script.empty()) {
            logger::warn("[DebugManager] OnConsoleRun: empty script");
            return;
        }

        PrismaView targetView = 0;
        if (!viewIdStr.empty()) {
            try { targetView = std::stoull(viewIdStr); } catch (...) {}
        }

        if (!targetView || !api_->IsValid(targetView)) {
            if (api_ && debugPanelView_) {
                api_->InteropCall(debugPanelView_, "consoleResult",
                    "{\"error\":\"Invalid or no view selected\"}");
            }
            return;
        }

        logger::info("[DebugManager] Console: running script on view {}: {}", targetView, script);

        PrismaView panelSnapshot = debugPanelView_;
        api_->Invoke(targetView, script.c_str(), [this, panelSnapshot](const char* result) {
            if (!api_ || !panelSnapshot || !api_->IsValid(panelSnapshot)) return;
            std::string r = result ? result : "";
            // JSON-escape result
            std::string escaped;
            for (char c : r) {
                if (c == '"')  escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else escaped += c;
            }
            std::string json = "{\"result\":\"" + escaped + "\"}";
            api_->InteropCall(panelSnapshot, "consoleResult", json.c_str());
        });
    }

    void DebugManager::OnViewsRefresh(const std::string&) noexcept {
        if (!api_ || !debugPanelView_ || !api_->IsValid(debugPanelView_)) return;
        std::string snapshot = ViewManager::GetViewsSnapshot();
        api_->InteropCall(debugPanelView_, "viewsData", snapshot.c_str());
    }

    void DebugManager::OnViewAction(const std::string& json) noexcept {
        // Expect JSON: {"viewId":"<id>","action":"toggleVisible"|"focus"}
        auto findStr = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = json.find(needle);
            if (pos == std::string::npos) return {};
            pos += needle.size();
            std::string val;
            while (pos < json.size() && json[pos] != '"') val += json[pos++];
            return val;
        };

        std::string viewIdStr = findStr("viewId");
        std::string action    = findStr("action");

        PrismaView targetView = 0;
        if (!viewIdStr.empty()) {
            try { targetView = std::stoull(viewIdStr); } catch (...) {}
        }

        if (!targetView || !api_ || !api_->IsValid(targetView)) {
            logger::warn("[DebugManager] OnViewAction: invalid viewId '{}'", viewIdStr);
            return;
        }

        if (action == "toggleVisible") {
            if (api_->IsHidden(targetView)) {
                api_->Show(targetView);
                logger::info("[DebugManager] View [{}] shown via debug panel", targetView);
            } else {
                api_->Hide(targetView);
                logger::info("[DebugManager] View [{}] hidden via debug panel", targetView);
            }
        } else if (action == "focus") {
            api_->Focus(targetView, false, false);
            logger::info("[DebugManager] View [{}] focused via debug panel", targetView);
        } else {
            logger::warn("[DebugManager] OnViewAction: unknown action '{}'", action);
        }

        // Refresh view list after action
        OnViewsRefresh("");
    }

}  // namespace PrismaUI::Debug
```

- [ ] **Step 2: Build to check for compile errors**

```powershell
cd "E:\F4SE OG\Prisma\PrismaUI_F4 New Gen"
.\build.bat 2>&1 | Select-String "error C" | Select-Object -First 20
```

Expected: no errors in the new files.

- [ ] **Step 3: Commit**

```bash
git add "src/Debug/DebugManager.cpp"
git commit -m "feat: DebugManager - init, event handler, view lifecycle, JS handlers"
```

---

### Task 5: Create `assets/views/debug_button.html`

**Files:**
- Create: `assets/views/debug_button.html`

- [ ] **Step 1: Write the file**

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
html, body {
    width: 100vw; height: 100vh;
    background: transparent;
    overflow: hidden;
    font-family: 'Consolas', 'Courier New', monospace;
}
#btn {
    position: fixed;
    bottom: 16px;
    right: 16px;
    width: 90px;
    height: 30px;
    background: rgba(15, 15, 15, 0.88);
    border: 1px solid rgba(255, 255, 255, 0.18);
    border-radius: 6px;
    color: #aaa;
    font-size: 11px;
    letter-spacing: 0.5px;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    user-select: none;
}
#btn:hover { background: rgba(35, 35, 35, 0.92); color: #fff; border-color: rgba(255,255,255,0.3); }
</style>
</head>
<body>
<div id="btn">≡ Debug</div>
<script>
const btn = document.getElementById('btn');
let isDragging = false, hasMoved = false;
let startX = 0, startY = 0, origLeft = 0, origTop = 0;

btn.addEventListener('mousedown', function(e) {
    isDragging = true;
    hasMoved   = false;
    const rect = btn.getBoundingClientRect();
    startX   = e.clientX;
    startY   = e.clientY;
    origLeft = rect.left;
    origTop  = rect.top;
    e.preventDefault();
});

document.addEventListener('mousemove', function(e) {
    if (!isDragging) return;
    const dx = e.clientX - startX;
    const dy = e.clientY - startY;
    if (Math.abs(dx) > 3 || Math.abs(dy) > 3) hasMoved = true;
    if (hasMoved) {
        btn.style.left   = Math.max(0, origLeft + dx) + 'px';
        btn.style.top    = Math.max(0, origTop  + dy) + 'px';
        btn.style.right  = 'auto';
        btn.style.bottom = 'auto';
    }
});

document.addEventListener('mouseup', function(e) {
    if (!isDragging) return;
    isDragging = false;
    if (hasMoved) {
        const rect = btn.getBoundingClientRect();
        prismaDebugSavePos(JSON.stringify({ x: Math.round(rect.left), y: Math.round(rect.top) }));
    } else {
        prismaDebugToggle('');
    }
});

// Called by C++ via InteropCall("setPos", json) to restore saved position
function setPos(json) {
    try {
        const pos = JSON.parse(json);
        if (typeof pos.x === 'number' && typeof pos.y === 'number') {
            btn.style.left   = pos.x + 'px';
            btn.style.top    = pos.y + 'px';
            btn.style.right  = 'auto';
            btn.style.bottom = 'auto';
        }
    } catch(e) { /* keep CSS default (bottom-right) */ }
}
</script>
</body>
</html>
```

- [ ] **Step 2: Commit**

```bash
git add "assets/views/debug_button.html"
git commit -m "feat: add debug_button.html - draggable overlay button"
```

---

### Task 6: Create `assets/views/debug_panel.html`

**Files:**
- Create: `assets/views/debug_panel.html`

- [ ] **Step 1: Write the file**

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
html, body {
    width: 100vw; height: 100vh;
    background: rgba(0, 0, 0, 0.65);
    font-family: 'Consolas', 'Courier New', monospace;
    color: #ddd;
    display: flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
}
#panel {
    width: 900px; height: 600px;
    background: #181818;
    border: 1px solid #333;
    border-radius: 8px;
    display: flex;
    flex-direction: column;
}
/* Header / tab bar */
#header {
    display: flex;
    align-items: center;
    gap: 2px;
    padding: 6px 10px;
    border-bottom: 1px solid #2a2a2a;
    background: #141414;
    border-radius: 8px 8px 0 0;
    flex-shrink: 0;
}
.tab-btn {
    padding: 3px 14px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 11px;
    color: #666;
    user-select: none;
}
.tab-btn.active { background: #242424; color: #e0e0e0; }
.tab-btn:hover:not(.active) { color: #aaa; }
#close-btn {
    margin-left: auto;
    cursor: pointer;
    color: #555;
    font-size: 15px;
    padding: 0 6px;
    user-select: none;
}
#close-btn:hover { color: #ccc; }

/* Tab content areas */
.tab-pane { display: none; flex: 1; flex-direction: column; padding: 8px; min-height: 0; }
.tab-pane.active { display: flex; }

/* ── Log tab ── */
#log-filter {
    background: #0f0f0f; border: 1px solid #2a2a2a; color: #ccc;
    padding: 4px 8px; border-radius: 4px; font-family: inherit; font-size: 11px;
    margin-bottom: 5px; width: 100%;
}
#log-controls {
    display: flex; gap: 8px; align-items: center;
    margin-bottom: 5px; font-size: 11px; flex-shrink: 0;
}
#log-controls label { color: #777; cursor: pointer; }
#log-controls button {
    background: #242424; border: 1px solid #3a3a3a; color: #888;
    padding: 2px 10px; border-radius: 3px; cursor: pointer; font-size: 10px;
}
#log-controls button:hover { color: #ccc; }
#log-count { color: #444; margin-left: auto; }
#log-scroll {
    flex: 1; overflow-y: auto; background: #0e0e0e;
    border: 1px solid #222; border-radius: 3px; padding: 3px 5px;
    min-height: 0;
}
.ll { font-size: 10px; padding: 1px 2px; white-space: pre-wrap; word-break: break-all; line-height: 1.4; }
.ll-debug    { color: #555; }
.ll-info     { color: #bbb; }
.ll-warn     { color: #e8a020; }
.ll-error    { color: #d44; }
.ll-critical { color: #f55; font-weight: bold; }

/* ── Console tab ── */
#view-select {
    background: #0f0f0f; border: 1px solid #2a2a2a; color: #bbb;
    padding: 4px 6px; border-radius: 4px; font-family: inherit; font-size: 11px;
    margin-bottom: 5px; width: 100%;
}
#script-input {
    background: #0f0f0f; border: 1px solid #2a2a2a; color: #ccc;
    padding: 6px; border-radius: 4px; resize: none; font-family: inherit; font-size: 11px;
    width: 100%; flex: 0 0 90px; margin-bottom: 4px;
}
#run-btn {
    align-self: flex-end;
    background: #1a2e1a; border: 1px solid #2e5c2e; color: #6fa86f;
    padding: 3px 18px; border-radius: 4px; cursor: pointer; font-size: 11px;
    margin-bottom: 5px;
}
#run-btn:hover { background: #213921; color: #9fc99f; }
#console-result {
    flex: 1; background: #0e0e0e; border: 1px solid #222; border-radius: 3px;
    padding: 6px; font-size: 11px; overflow-y: auto; color: #6fa86f;
    white-space: pre-wrap; word-break: break-all; min-height: 0;
}

/* ── Views tab ── */
#views-refresh {
    background: #242424; border: 1px solid #3a3a3a; color: #888;
    padding: 3px 12px; border-radius: 3px; cursor: pointer; font-size: 11px;
    margin-bottom: 6px; align-self: flex-start;
}
#views-refresh:hover { color: #ccc; }
#views-scroll { flex: 1; overflow-y: auto; min-height: 0; }
#views-table { width: 100%; border-collapse: collapse; font-size: 11px; }
#views-table th {
    background: #1a1a1a; padding: 4px 8px; text-align: left;
    border-bottom: 1px solid #2a2a2a; color: #666; position: sticky; top: 0;
}
#views-table td { padding: 4px 8px; border-bottom: 1px solid #1a1a1a; }
.act-btn {
    background: #242424; border: 1px solid #3a3a3a; color: #888;
    padding: 1px 8px; border-radius: 3px; cursor: pointer; font-size: 10px; margin-right: 3px;
}
.act-btn:hover { color: #ccc; }
</style>
</head>
<body>
<div id="panel">
    <div id="header">
        <span class="tab-btn active" onclick="switchTab('log')">Log</span>
        <span class="tab-btn"        onclick="switchTab('console')">Console</span>
        <span class="tab-btn"        onclick="switchTab('views')">Views</span>
        <span id="close-btn"         onclick="prismaDebugToggle('')">✕</span>
    </div>

    <!-- LOG TAB -->
    <div id="pane-log" class="tab-pane active">
        <input id="log-filter" placeholder="Filter…" oninput="applyFilter(this.value)">
        <div id="log-controls">
            <label><input type="checkbox" id="pin" checked> Pin to bottom</label>
            <button onclick="clearLog()">Clear</button>
            <span id="log-count">0 lines</span>
        </div>
        <div id="log-scroll"></div>
    </div>

    <!-- CONSOLE TAB -->
    <div id="pane-console" class="tab-pane">
        <select id="view-select"><option value="">— select view —</option></select>
        <textarea id="script-input" placeholder="JavaScript to execute…" rows="5"></textarea>
        <button id="run-btn" onclick="runScript()">▶ Run</button>
        <div id="console-result"></div>
    </div>

    <!-- VIEWS TAB -->
    <div id="pane-views" class="tab-pane">
        <button id="views-refresh" onclick="prismaViewsRefresh('')">↻ Refresh</button>
        <div id="views-scroll">
            <table id="views-table">
                <thead><tr>
                    <th>ID</th><th>URL</th><th>Visible</th><th>Focused</th><th>Order</th><th>Actions</th>
                </tr></thead>
                <tbody id="views-body"></tbody>
            </table>
        </div>
    </div>
</div>

<script>
// ── Tab switching ──────────────────────────────────────────────
function switchTab(name) {
    document.querySelectorAll('.tab-btn').forEach((el, i) => {
        const names = ['log','console','views'];
        el.classList.toggle('active', names[i] === name);
    });
    ['log','console','views'].forEach(n => {
        const el = document.getElementById('pane-' + n);
        if (el) el.classList.toggle('active', n === name);
    });
    if (name === 'views') prismaViewsRefresh('');
}

// ── Log tab ───────────────────────────────────────────────────
const logLines = [];
let filterTerm = '';

function appendLog(jsonLine) {
    try {
        const e = JSON.parse(jsonLine);
        logLines.push(e);
        if (logLines.length > 1000) logLines.shift();
        if (!filterTerm || e.msg.toLowerCase().includes(filterTerm))
            renderLine(e);
        updateCount();
        scrollIfPinned();
    } catch (_) {}
}

function bulkLog(jsonArray) {
    try {
        const arr = JSON.parse(jsonArray);
        logLines.length = 0;
        document.getElementById('log-scroll').innerHTML = '';
        arr.forEach(e => {
            logLines.push(e);
            if (!filterTerm || e.msg.toLowerCase().includes(filterTerm))
                renderLine(e);
        });
        updateCount();
        scrollIfPinned();
    } catch (_) {}
}

function renderLine(e) {
    const div = document.createElement('div');
    div.className = 'll ll-' + (e.level || 'info');
    div.textContent = '[' + (e.ts || '') + '] ' + (e.msg || '');
    document.getElementById('log-scroll').appendChild(div);
}

function applyFilter(text) {
    filterTerm = text.toLowerCase();
    const scroll = document.getElementById('log-scroll');
    scroll.innerHTML = '';
    logLines.filter(e => !filterTerm || e.msg.toLowerCase().includes(filterTerm))
            .forEach(renderLine);
    updateCount();
}

function clearLog() {
    logLines.length = 0;
    document.getElementById('log-scroll').innerHTML = '';
    updateCount();
}

function updateCount() {
    document.getElementById('log-count').textContent = logLines.length + ' lines';
}

function scrollIfPinned() {
    if (document.getElementById('pin').checked) {
        const s = document.getElementById('log-scroll');
        s.scrollTop = s.scrollHeight;
    }
}

// ── Console tab ───────────────────────────────────────────────
const cmdHistory = [];
let histIdx = -1;

const scriptInput = document.getElementById('script-input');
scriptInput.addEventListener('keydown', function(e) {
    if (e.key === 'ArrowUp' && !e.shiftKey) {
        e.preventDefault();
        if (histIdx < cmdHistory.length - 1) {
            histIdx++;
            scriptInput.value = cmdHistory[cmdHistory.length - 1 - histIdx];
        }
    } else if (e.key === 'ArrowDown' && !e.shiftKey) {
        e.preventDefault();
        if (histIdx > 0) {
            histIdx--;
            scriptInput.value = cmdHistory[cmdHistory.length - 1 - histIdx];
        } else {
            histIdx = -1;
            scriptInput.value = '';
        }
    }
});

function runScript() {
    const viewId = document.getElementById('view-select').value;
    const script = scriptInput.value.trim();
    if (!script) return;
    cmdHistory.push(script);
    if (cmdHistory.length > 20) cmdHistory.shift();
    histIdx = -1;
    document.getElementById('console-result').textContent = '…running';
    prismaConsoleRun(JSON.stringify({ viewId: viewId, script: script }));
}

function consoleResult(json) {
    try {
        const d = JSON.parse(json);
        const el = document.getElementById('console-result');
        if (d.error) {
            el.style.color = '#d44';
            el.textContent = '⚠ ' + d.error;
        } else {
            el.style.color = '#6fa86f';
            el.textContent = '→ ' + d.result;
        }
    } catch (_) {
        document.getElementById('console-result').textContent = json;
    }
}

// ── Views tab ─────────────────────────────────────────────────
function viewsData(json) {
    try {
        const views = JSON.parse(json);
        const tbody = document.getElementById('views-body');
        tbody.innerHTML = '';

        const sel = document.getElementById('view-select');
        const prev = sel.value;
        sel.innerHTML = '<option value="">— select view —</option>';

        views.forEach(function(v) {
            const urlShort = (v.url || '').split('/').pop() || v.url;
            const tr = document.createElement('tr');
            tr.innerHTML =
                '<td>' + v.id + '</td>' +
                '<td title="' + v.url + '">' + urlShort + '</td>' +
                '<td>' + (v.visible ? '✓' : '—') + '</td>' +
                '<td>' + (v.focused ? '✓' : '—') + '</td>' +
                '<td>' + v.order + '</td>' +
                '<td>' +
                    '<button class="act-btn" onclick=\'prismaViewAction(JSON.stringify({viewId:"' + v.id + '",action:"toggleVisible"}))\'>Toggle</button>' +
                    '<button class="act-btn" onclick=\'prismaViewAction(JSON.stringify({viewId:"' + v.id + '",action:"focus"}))\'>Focus</button>' +
                '</td>';
            tbody.appendChild(tr);

            const opt = document.createElement('option');
            opt.value = v.id;
            opt.textContent = v.id + ' — ' + urlShort;
            sel.appendChild(opt);
        });

        if (prev) sel.value = prev;
    } catch (_) {}
}
</script>
</body>
</html>
```

- [ ] **Step 2: Commit**

```bash
git add "assets/views/debug_panel.html"
git commit -m "feat: add debug_panel.html - log/console/views 3-tab debug panel"
```

---

### Task 7: Wire `main.cpp` + final build

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add the include**

Add after the existing includes at the top of `src/main.cpp`:

```cpp
#include "Debug/DebugManager.h"
```

- [ ] **Step 2: Call `Init` in kGameDataReady**

In `F4SEMessageHandler`, inside the `kGameDataReady` branch (after `Hooks::D3DHooks::Install()`):

```cpp
    if (message->type == F4SE::MessagingInterface::kGameDataReady) {
        logger::info("F4SE: kGameDataReady — installing D3D hooks");
        PrismaUI::ConflictChecker::CheckPreHooks();
        Hooks::D3DHooks::Install();
        // Init debug manager (registers log sink + MenuOpenCloseEvent)
        auto* api = static_cast<PRISMA_UI_API::IVPrismaUI3*>(
            PluginAPI::PrismaUIInterface::GetSingleton());
        PrismaUI::Debug::DebugManager::GetSingleton().Init(api);
    }
```

- [ ] **Step 3: Call `OnPostLoadGame` in kPostLoadGame and kNewGame**

In `F4SEMessageHandler`, add handlers for `kPostLoadGame` and `kNewGame`:

```cpp
    if (message->type == F4SE::MessagingInterface::kPostLoadGame ||
        message->type == F4SE::MessagingInterface::kNewGame) {
        PrismaUI::Debug::DebugManager::GetSingleton().OnPostLoadGame();
    }
```

- [ ] **Step 4: Full clean build**

```powershell
cd "E:\F4SE OG\Prisma\PrismaUI_F4 New Gen"
.\build.bat 2>&1 | Tee-Object build_out.txt
```

Expected: build succeeds, `dist\PrismaUI_F4_1.0.0\F4SE\plugins\PrismaUI_F4.dll` produced.

If there are errors, read the last 40 lines of `build_out.txt` to diagnose.

- [ ] **Step 5: Commit**

```bash
git add "src/main.cpp"
git commit -m "feat: wire DebugManager into F4SE message handler - init + view lifecycle"
```

---

## Self-Review

**Spec coverage:**
- DebugLogSink ring buffer (1000 lines, push-only-when-open): ✅ Task 1
- DebugManager Init (log sink install, MenuOpenCloseEvent): ✅ Task 4
- View creation on kPostLoadGame/kNewGame: ✅ Task 4
- PauseMenu open/close → show/hide button + panel: ✅ Task 4 (`ProcessEvent`)
- draggable button, default bottom-right: ✅ Task 5
- Position save/load JSON: ✅ Task 4 (`SaveButtonPos`/`SendButtonPos`) + Task 5 (JS drag)
- Panel open: SetPushCallback + Show + Focus + bulkLog: ✅ Task 4 (`OpenPanel`)
- Panel close: ClearPushCallback + Unfocus + Hide: ✅ Task 4 (`ClosePanel`)
- Log tab with appendLog/bulkLog, filter, pin, clear: ✅ Task 6
- Console tab with view dropdown, history, result: ✅ Task 6
- Views tab with refresh, toggle, focus: ✅ Task 6
- GetViewsSnapshot(): ✅ Task 2
- Wire into main.cpp: ✅ Task 7

**Placeholder scan:** No TBDs or vague steps. All code blocks are complete.

**Type consistency:**
- `PrismaView` is `uint64_t` throughout — consistent
- `PRISMA_UI_API::IVPrismaUI3*` passed to Init, stored as `api_` — consistent
- `prismaDebugToggle`, `prismaDebugSavePos`, `prismaConsoleRun`, `prismaViewsRefresh`, `prismaViewAction` — names match between HTML (called as functions) and C++ (registered as listeners) in Tasks 4, 5, 6
- `appendLog(jsonLine)`, `bulkLog(jsonArray)`, `consoleResult(json)`, `viewsData(json)` — names match between C++ `InteropCall` calls in Task 4 and JS function definitions in Task 6
- `setPos(json)` — matches between `SendButtonPos` InteropCall in Task 4 and `function setPos(json)` in Task 5
