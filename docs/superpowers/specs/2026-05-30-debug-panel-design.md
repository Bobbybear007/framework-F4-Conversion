# In-Game Debug Panel — Design Spec
**Date:** 2026-05-30
**Project:** PrismaUI_F4 New Gen

---

## Overview

An in-game debugger/logger viewer surfaced through a draggable overlay button that appears when the Fallout 4 pause menu (PauseMenu) is open. The panel has three tabs: real-time log viewer, JS console, and active view inspector. Designed for zero performance impact during normal gameplay — no Ultralight rendering occurs until the player actually opens the panel.

---

## New Files

| File | Purpose |
|---|---|
| `src/Debug/DebugLogSink.h` | Custom spdlog sink — ring buffer + conditional push to panel |
| `src/Debug/DebugManager.h` | Public interface — Init, OnPauseMenuOpen/Close, view lifecycle |
| `src/Debug/DebugManager.cpp` | MenuOpenCloseEvent sink, view creation, JS listener wiring |
| `assets/views/debug_button.html` | Draggable "≡ Debug" overlay button |
| `assets/views/debug_panel.html` | Full debug panel (Log / Console / Views tabs) |

**Modified:**
- `src/main.cpp` — call `DebugManager::Init()` on `kGameDataReady`, create views on `kPostLoadGame`/`kNewGame`

---

## Architecture

```
spdlog file sink  →  PrismaUI_F4.log  (unchanged)
spdlog DebugLogSink → ring buffer (1000 lines, always running, cheap)
                         └─ InteropCall("appendLog") only when panel is open
```

`DebugManager` owns both Ultralight views and handles all lifetime management. The `MenuOpenCloseEvent` sink is registered on `kGameDataReady`. Views are (re)created on `kPostLoadGame`/`kNewGame` following existing PrismaUI patterns.

---

## DebugLogSink

**Header-only** (`DebugLogSink.h`), inherits `spdlog::sinks::base_sink<std::mutex>`.

- Ring buffer: `std::deque<std::string>` capped at **1000 entries**
- Each entry: `{"level":"info","msg":"...","ts":"HH:MM:SS.mmm"}`
- `sink_it_()` — appends to ring buffer; if `panelVisible_` is true AND `panelViewId_` is valid, calls `InteropCall(panelViewId_, "appendLog", jsonLine)`
- `SetPanelActive(PrismaViewId id, bool active)` — sets `panelViewId_` and `panelVisible_` atomically
- `GetAllLogs()` → `std::string` — serializes entire ring buffer as a JSON array for bulk send on panel open
- `Clear()` — clears ring buffer

**Performance contract:** When panel is closed, `sink_it_` does one deque push + one size check. No Ultralight calls, no JSON serialization.

---

## DebugManager

### Init (called on `kGameDataReady`)
1. Instantiate `DebugLogSink` and add it to spdlog's default logger via `spdlog::default_logger()->sinks().push_back(sink)`
2. Register `MenuOpenCloseEvent` sink: `RE::UI::GetSingleton()->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(this)`
   - `DebugManager` must inherit `RE::BSTEventSink<RE::MenuOpenCloseEvent>` and implement `ProcessEvent(const RE::MenuOpenCloseEvent&, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)`

### View creation (called on `kPostLoadGame` and `kNewGame`)
1. `debugButtonView = api->CreateView("debug_button.html", onDomReady)` → immediately `Hide`
2. `debugPanelView = api->CreateView("debug_panel.html", onDomReady)` → immediately `Hide`
3. In `onDomReady` for button: `RegisterJSListener("prismaDebugToggle", ...)`, `RegisterJSListener("prismaDebugSavePos", ...)`
4. In `onDomReady` for panel: `RegisterJSListener("prismaDebugReady", ...)`, `RegisterJSListener("prismaConsoleRun", ...)`, `RegisterJSListener("prismaViewsRefresh", ...)`, `RegisterJSListener("prismaViewAction", ...)`

### MenuOpenCloseEvent handler
- Check `event.menuName == RE::PauseMenu::MENU_NAME` (string `"PauseMenu"` in F4SE)
- `opening == true` → `api->Show(debugButtonView)` + load saved button position via `InteropCall("setPos", json)`
- `opening == false` → `api->Hide(debugButtonView)` + `ClosePanel()`

### OpenPanel / ClosePanel
**OpenPanel:**
1. `sink->SetPanelActive(debugPanelView, true)` — enables live push
2. `api->Show(debugPanelView)`
3. `api->Focus(debugPanelView, false, true)` — no game pause, FocusMenu disabled (SystemMenu already owns cursor layer)
4. `api->InteropCall(debugPanelView, "bulkLog", sink->GetAllLogs())` — populate log tab

**ClosePanel:**
1. `sink->SetPanelActive(0, false)` — disables live push
2. `api->Unfocus(debugPanelView)`
3. `api->Hide(debugPanelView)`

---

## debug_button.html

- Fixed `width: 80px; height: 32px` pill button, label "≡ Debug"
- Draggable via `mousedown`/`mousemove`/`mouseup` — pure JS, no C++
- Default spawn position: **bottom-right** (`right: 16px; bottom: 16px`)
- On `mouseup` after drag: calls `prismaDebugSavePos({x, y})` → C++ writes to `Data/PrismaUI_F4/debug_button_pos.json`
- On `prismaDebugToggle` (JS `onclick`): calls `window.prismaDebugToggle("")`
- On `setPos(json)` call from C++: applies saved position

**Position persistence:**
- C++ reads `debug_button_pos.json` on `kPostLoadGame` and sends position to button via `InteropCall("setPos", json)` after DOM ready
- Falls back to bottom-right if file missing or malformed

---

## debug_panel.html

Full-screen semi-transparent dark overlay, centered content panel (~900×600px). Three tab buttons across the top.

### Log Tab (default active)
- Scrolling `<div>` of log lines, max 1000 displayed
- Color coding: `debug`=`#888`, `info`=`#eee`, `warn`=`#f5a623`, `error`=`#e74c3c`, `critical`=`#e74c3c bold`
- "Pin to bottom" toggle checkbox (auto-scroll when enabled)
- "Clear" button — clears display list only, not ring buffer
- Live text filter input — hides lines that don't contain substring
- `appendLog(jsonLine)` — called by C++ for each new line when panel open
- `bulkLog(jsonArray)` — called once on panel open to populate history

### Console Tab
- Dropdown: populated by `prismaViewsRefresh` response — shows `"<id> — <basename of URL>"` per view
- `<textarea>` for JS input (multiline)
- "Run" button → calls `prismaConsoleRun({viewId, script})`
- C++ calls `api->Invoke(targetView, script, callback)` → result sent back via `InteropCall(debugPanelView, "consoleResult", json)`
- Result area: shows return value or error string
- Command history: up/down arrow keys in textarea cycle last 20 commands (stored in JS array)

### Views Tab
- Table columns: **ID** | **URL** | **Visible** | **Focused** | **Order**
- "Refresh" button → calls `prismaViewsRefresh("")`
- C++ handler: iterates `Core::views`, serializes to JSON array, calls `InteropCall(debugPanelView, "viewsData", json)`
- Each row has **Toggle Visible** and **Focus** action buttons → call `prismaViewAction({viewId, action})` where action is `"toggleVisible"` or `"focus"`

---

## C++ View Action Handlers

| JS event | C++ handler |
|---|---|
| `prismaDebugToggle` | `OpenPanel()` or `ClosePanel()` based on current state |
| `prismaDebugSavePos({x,y})` | Write `debug_button_pos.json` |
| `prismaConsoleRun({viewId, script})` | `api->Invoke(viewId, script, resultCallback)` |
| `prismaViewsRefresh` | Serialize `Core::views` map → `InteropCall(panel, "viewsData", json)` |
| `prismaViewAction({viewId, action})` | Dispatch to `Show/Hide/Focus` |

---

## Performance Guarantees

| Scenario | Cost |
|---|---|
| Panel closed, normal gameplay | One `deque::push_back` + size check per log line. No Ultralight calls. |
| Panel open | `InteropCall` per new log line (amortized — log lines are infrequent during normal use) |
| Panel opening | One `GetAllLogs()` serialization + one `bulkLog` `InteropCall` |
| Button visible (pause menu open, panel closed) | Zero extra cost — button view is idle HTML |

---

## Out of Scope

- Scaleform/Flash injection into SystemMenu
- Remote debugging over network
- Breakpoints or step-through execution
- Log filtering by level (stretch goal — filter input covers the common case)
