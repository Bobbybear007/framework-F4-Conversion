# Examples

Copy-paste patterns for common PrismaUI_F4 integration scenarios.

---

## 1. Minimal Toggle Menu

The simplest possible plugin: one view, one hotkey, show/hide.

**C++:**
```cpp
#include "PrismaUI_F4_API.h"
#include "KeyHandler.h"

static PRISMA_UI_API::IVPrismaUI4* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void OnDomReady(PrismaView view) {
    g_api->RegisterConsoleCallback(view,
        [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
            logger::info("[JS] {}", msg);
        });

    // BindUIEvent — fires on game thread, RE:: access safe directly
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_visible = false;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    g_api->Invoke(view, "init()");
}

static void Toggle() {
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
        if (!g_api) { logger::error("PrismaUI_F4 not found"); return; }
        KeyHandler::RegisterSink();
        KeyHandler::GetSingleton()->Register(0x43, KeyEventType::KEY_DOWN, Toggle); // F9
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api && (!g_view || !g_api->IsValid(g_view))) {
            g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady);
            g_api->Hide(g_view);
        }
        break;
    }
}
```

**HTML:**
```html
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8">
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body { width:100vw; height:100vh; background:transparent;
         display:flex; align-items:center; justify-content:center;
         font-family:'Courier New',monospace; }
  .panel { background:rgba(0,0,0,0.9); border:1px solid #00661a;
           padding:40px; color:#00ff41; text-align:center; }
  button { margin-top:20px; padding:10px 24px; background:transparent;
           border:1px solid #00661a; color:#00ff41;
           font-family:'Courier New',monospace; cursor:pointer; }
  button:hover { background:rgba(0,255,65,0.1); }
</style>
</head>
<body>
<div class="panel">
  <h2>MY MENU</h2>
  <button onclick="window.requestClose()">CLOSE</button>
</div>
<script>
  window.init = () => console.log('menu ready');
</script>
</body>
</html>
```

---

## 2. JS → C++ with Game State (BindUIEvent)

Use `BindUIEvent` when the callback needs to read or write game state. RE:: access is safe directly inside the callback — no `AddTask` needed.

**C++:**
```cpp
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Helper.h"  // GetJsonString, GetJsonInt

static void OnDomReady(PrismaView view) {
    // Receive item equip request from JS
    g_api->BindUIEvent(view, "onItemEquip", [](const char* data) {
        // Already on game thread — RE:: access is safe
        uint32_t formId = static_cast<uint32_t>(
            PRISMA_UI_HELPER::GetJsonInt(data ? data : "", "formId"));
        auto* form = RE::TESForm::GetFormByID(formId);
        if (!form) return;
        logger::info("Equip: {}", form->GetFullName());
        // ... apply equip logic
    });

    // Receive a simple message string
    g_api->BindUIEvent(view, "sendMessage", [](const char* data) {
        std::string msg = PRISMA_UI_HELPER::GetJsonString(data ? data : "", "text");
        logger::info("From JS: {}", msg);
    });
}
```

**JS:**
```javascript
// Equip button click
document.getElementById('equipBtn').onclick = function() {
    window.onItemEquip(JSON.stringify({ formId: 0x1234 }));
};

// Text input send
document.getElementById('sendBtn').onclick = function() {
    const text = document.getElementById('input').value;
    window.sendMessage(JSON.stringify({ text }));
};
```

---

## 3. Pushing Structured Data to a View

Push game data to the HTML page using `InteropCall` with JSON.

**C++:**
```cpp
static void PushInventoryData(PrismaView view) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    std::string json = "[";
    bool first = true;
    player->GetInventory([&](RE::TESBoundObject& obj, const RE::InventoryEntryData& entry) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + std::string(obj.GetFullName()) + "\""
              + ",\"count\":" + std::to_string(entry.countDelta)
              + ",\"weight\":" + std::to_string(obj.GetWeight())
              + "}";
    });
    json += "]";

    g_api->InteropCall(view, "loadInventory", json.c_str());
}
```

**JS:**
```javascript
function loadInventory(jsonStr) {
    const items = JSON.parse(jsonStr);
    const list  = document.getElementById('list');
    list.innerHTML = items.map(item =>
        `<div class="row">
           <span class="name">${escapeHtml(item.name)}</span>
           <span class="count">x${item.count}</span>
         </div>`
    ).join('');
}

function escapeHtml(str) {
    return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
```

---

## 4. Translations (RegisterTranslations)

Load Fallout 4 translation files and expose them as `window.t()` in your page.

**C++ — register immediately after CreateView:**
```cpp
case F4SE::MessagingInterface::kPostLoadGame:
case F4SE::MessagingInterface::kNewGame:
    if (g_api && (!g_view || !g_api->IsValid(g_view))) {
        g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady);
        g_api->RegisterTranslations(g_view, "MyPlugin_F4");  // must be before DOM ready
        g_api->Hide(g_view);
    }
    break;
```

**Translation file** (`Data/Interface/Translations/MyPlugin_F4_en.txt`, UTF-16 LE with BOM):
```
$MENU_TITLE	My Plugin
$CLOSE_BTN	Close
$ITEM_LABEL	Item: {0}
```

**JS — use `window.t()` anywhere in the page:**
```javascript
document.getElementById('title').textContent  = window.t('$MENU_TITLE');
document.getElementById('close').textContent  = window.t('$CLOSE_BTN');

// For keys not found, t() returns the key itself — safe to call unconditionally
console.log(window.t('$MISSING_KEY')); // → "$MISSING_KEY"
```

The framework auto-detects the game language from INI files. If `MyPlugin_F4_de.txt` exists and the game is set to German, German strings are injected. Falls back to `en` if the language file is not found.

---

## 5. Reading a Value Back from JS

`Invoke` with a callback pulls a value from the page's JS state.

```cpp
g_api->Invoke(view,
    "document.getElementById('volumeSlider').value",
    [](const char* result) {
        int volume = std::atoi(result ? result : "0");
        logger::info("Volume is {}", volume);
    });
```

For passing complex state back, have JS call a `BindUIEvent` listener — it is cleaner than parsing `Invoke` results.

---

## 6. Four Views, Four Keys

Managing multiple views from a single plugin.

```cpp
static constexpr uint32_t       KEYS[4]  = { 0x43, 0x44, 0x57, 0x58 };
static constexpr const char*    FILES[4] = {
    "Interface/MyPlugin/levelup.html",
    "Interface/MyPlugin/mcm.html",
    "Interface/MyPlugin/companion.html",
    "Interface/MyPlugin/terminal.html"
};

static PRISMA_UI_API::IVPrismaUI4* g_api      = nullptr;
static PrismaView                   g_views[4] = {};
static bool                         g_visible[4] = {};

static void Toggle(int idx) {
    if (!g_api || !g_api->IsValid(g_views[idx])) return;
    g_visible[idx] = !g_visible[idx];
    if (g_visible[idx]) {
        g_api->Show(g_views[idx]);
        g_api->Focus(g_views[idx], true);
    } else {
        g_api->Unfocus(g_views[idx]);
        g_api->Hide(g_views[idx]);
    }
}

static void CreateViews() {
    for (int i = 0; i < 4; i++) {
        if (g_views[i] && g_api->IsValid(g_views[i])) continue;
        g_views[i] = g_api->CreateView(FILES[i], nullptr);
        g_api->RegisterConsoleCallback(g_views[i],
            [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
                logger::info("[JS] {}", msg);
            });
        g_api->Hide(g_views[i]);
    }
}
```

---

## 7. Z-Ordering Two Overlapping Views

```cpp
// Background HUD — always visible, no focus, low z-order
g_hudView = g_api->CreateView("Interface/MyPlugin/hud.html", nullptr);
g_api->SetOrder(g_hudView, 0);
g_api->Show(g_hudView);

// Popup menu — appears on top of HUD when opened
g_menuView = g_api->CreateView("Interface/MyPlugin/menu.html", OnMenuReady);
g_api->SetOrder(g_menuView, 10);
g_api->Hide(g_menuView);
```

---

## 8. Live HUD Updates

Push stats to a HUD on a recurring schedule.

```cpp
static void ScheduleHudUpdate() {
    F4SE::GetTaskInterface()->AddTask([]() {
        if (!g_api || !g_api->IsValid(g_hudView) || g_api->IsHidden(g_hudView)) {
            ScheduleHudUpdate();
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) { ScheduleHudUpdate(); return; }

        std::string json =
            "{\"hp\":" + std::to_string((int)player->GetActorValue(RE::ActorValue::kHealth))
          + ",\"ap\":" + std::to_string((int)player->GetActorValue(RE::ActorValue::kActionPoints))
          + "}";
        g_api->InteropCall(g_hudView, "updateStats", json.c_str());
        ScheduleHudUpdate();
    });
}
```

Hook a game timer or tick event rather than a tight recursion loop in production.

---

## 9. Inspector Setup (Development Only)

```cpp
#ifdef PRISMA_DEV
api->CreateInspectorView(view);
api->SetInspectorBounds(view, 960.0f, 0.0f, 960, 600);
api->SetInspectorVisibility(view, true);

KeyHandler::GetSingleton()->Register(0x58, KeyEventType::KEY_DOWN, []() {
    api->SetInspectorVisibility(g_view, !api->IsInspectorVisible(g_view));
});
#endif
```

Remove all `CreateInspectorView` calls before shipping.

---

## 10. Error-Resilient View Creation

```cpp
static void CreateViews() {
    if (!g_api) {
        logger::error("PrismaUI not available — is PrismaUI_F4.dll installed?");
        return;
    }
    if (g_view && g_api->IsValid(g_view)) return;

    g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady);
    if (!g_api->IsValid(g_view)) {
        logger::error("View invalid immediately — check that the HTML path is correct");
        g_view = 0;
        return;
    }

    g_api->RegisterConsoleCallback(g_view,
        [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
            if (lvl == PRISMA_UI_API::ConsoleMessageLevel::Error)
                logger::error("[JS ERR] {}", msg);
            else
                logger::info("[JS] {}", msg);
        });
    g_api->Hide(g_view);
    logger::info("View created (id={})", g_view);
}
```

Common failure modes:
- `g_api` is null → PrismaUI_F4 is not installed or the version is too old
- `IsValid` is false immediately → HTML file path is wrong
- JS errors in console → syntax errors or undefined functions in your page
