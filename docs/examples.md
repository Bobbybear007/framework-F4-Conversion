# Examples

Real, tested patterns extracted from PrismaInventory_F4 and PrismaShowcase_F4.

---

## 1. Minimal Toggle Menu

The simplest possible plugin: one view, one hotkey, show/hide with game pause.

**C++ (main.cpp):**
```cpp
#include "PrismaUI_F4_API.h"
#include "KeyHandler.h"
#include <spdlog/sinks/basic_file_sink.h>

static PRISMA_UI_API::IVPrismaUI2* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void OnDomReady(PrismaView /*view*/) {
    logger::info("DOM ready");
}

static void Toggle() {
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, true, false);  // pauseGame=true
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
        KeyHandler::RegisterSink();
        KeyHandler::GetSingleton()->Register(0x43, KeyEventType::KEY_DOWN, Toggle); // F9
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api && g_view == 0) {
            g_view = g_api->CreateView("mymenu.html", OnDomReady);
            g_api->RegisterConsoleCallback(g_view,
                [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
                    logger::info("[JS] {}", msg);
                });
            g_api->Hide(g_view);
        }
        break;
    }
}
```

**HTML (mymenu.html):**
```html
<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8">
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body { width:100vw; height:100vh; background:transparent;
         display:flex; align-items:center; justify-content:center;
         font-family:'Courier New',monospace; }
  .panel { background:rgba(0,0,0,0.9); border:1px solid #00661a;
           padding:40px; color:#00ff41; text-align:center; }
  button { margin-top:20px; padding:10px 24px; background:transparent;
           border:1px solid #00661a; color:#00ff41; font-family:'Courier New',monospace;
           font-size:12px; cursor:pointer; letter-spacing:2px; }
  button:hover { background:rgba(0,255,65,0.1); }
</style>
</head>
<body>
<div class="panel">
  <h2 style="letter-spacing:3px">MY MENU</h2>
  <p style="margin-top:12px;color:#009921;font-size:11px">Game is paused.</p>
  <button onclick="requestClose()">CLOSE</button>
</div>
<script>
  // requestClose is registered from C++ via RegisterJSListener
  console.log('mymenu ready');
</script>
</body>
</html>
```

**Wire up close from C++ in OnDomReady:**
```cpp
static void OnDomReady(PrismaView /*view*/) {
    g_api->RegisterJSListener(g_view, "requestClose", [](const char*) {
        g_visible = false;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
}
```

---

## 2. Pushing Structured Data to a View

Push game data to the HTML page using `InteropCall` with JSON.

**C++ — build and push inventory:**
```cpp
#include <nlohmann/json.hpp>  // or build JSON manually with sprintf

static void PushInventoryData(PrismaView view) {
    // Collect items from game
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    std::string json = "[";
    bool first = true;
    player->GetInventory([&](RE::TESBoundObject& obj, const RE::InventoryEntryData& entry) {
        if (!first) json += ",";
        first = false;
        // Escape name for JSON (simplified — use a proper JSON lib in production)
        json += "{\"name\":\"" + std::string(obj.GetFullName()) + "\""
              + ",\"count\":" + std::to_string(entry.countDelta)
              + ",\"weight\":" + std::to_string(obj.GetWeight())
              + "}";
    });
    json += "]";

    g_api->InteropCall(view, "loadInventory", json.c_str());
}
```

**HTML — receive and render:**
```html
<div id="list"></div>
<script>
function loadInventory(jsonStr) {
  var items = JSON.parse(jsonStr);
  var html = '';
  items.forEach(function(item) {
    html += '<div class="row">'
          + '<span class="name">' + escapeHtml(item.name) + '</span>'
          + '<span class="count">x' + item.count + '</span>'
          + '</div>';
  });
  document.getElementById('list').innerHTML = html;
}

function escapeHtml(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
</script>
```

---

## 3. Receiving Events from JavaScript

The JS → C++ direction uses `RegisterJSListener`. Each listener is a global function injected into the page's JS context.

**C++ — register multiple listeners:**
```cpp
static void OnDomReady(PrismaView view) {
    // Close button
    g_api->RegisterJSListener(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
        g_visible = false;
    });

    // Setting changed
    g_api->RegisterJSListener(view, "onSettingChanged", [](const char* json) {
        // e.g. json = '{"key":"hungerRate","value":"75"}'
        logger::info("Setting: {}", json);
        // Parse and apply in-game...
    });

    // Item selected
    g_api->RegisterJSListener(view, "onItemSelected", [](const char* formIdStr) {
        uint32_t formId = std::stoul(formIdStr, nullptr, 16);
        auto* form = RE::TESForm::GetFormByID(formId);
        if (form) logger::info("Selected: {}", form->GetFullName());
    });
}
```

**HTML — call from buttons and inputs:**
```javascript
// Close
document.getElementById('closeBtn').onclick = function() {
  requestClose();
};

// Setting slider
document.getElementById('hungerSlider').oninput = function() {
  onSettingChanged(JSON.stringify({ key: 'hungerRate', value: this.value }));
};

// Item click
document.querySelectorAll('.item-row').forEach(function(el) {
  el.onclick = function() {
    onItemSelected(this.dataset.formid);
  };
});
```

---

## 4. Reading a Value Back from JS

`Invoke` with a callback lets you pull a value from the page's JS state.

```cpp
// Ask the page what the current slider value is
g_api->Invoke(view,
    "document.getElementById('volumeSlider').value",
    [](const char* result) {
        int volume = std::atoi(result);
        logger::info("Volume is {}", volume);
        // Apply to game audio...
    });
```

For passing complex state back, have JS call a listener instead — it's cleaner than parsing Invoke results.

---

## 5. Four Views, Four Keys (Showcase Pattern)

Managing multiple views from a single plugin. Each view is independent.

```cpp
static constexpr uint32_t KEYS[4]        = { 0x43, 0x44, 0x57, 0x58 }; // F9-F12
static constexpr const char* FILES[4]    = { "levelup.html", "mcm.html",
                                              "companion.html", "terminal.html" };
static constexpr const char* NAMES[4]    = { "LevelUp", "MCM", "Companion", "Terminal" };

static PRISMA_UI_API::IVPrismaUI2* g_api    = nullptr;
static PrismaView                   g_views[4] = {};
static bool                         g_visible[4] = {};

static void Toggle(int idx) {
    if (!g_api || !g_api->IsValid(g_views[idx])) return;
    g_visible[idx] = !g_visible[idx];
    if (g_visible[idx]) {
        g_api->Show(g_views[idx]);
        g_api->Focus(g_views[idx], true, false);
    } else {
        g_api->Unfocus(g_views[idx]);
        g_api->Hide(g_views[idx]);
    }
}

static void CreateViews() {
    for (int i = 0; i < 4; i++) {
        if (g_views[i] != 0) continue;
        g_views[i] = g_api->CreateView(FILES[i], nullptr);
        g_api->RegisterConsoleCallback(g_views[i],
            [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
                logger::info("[JS] {}", msg);
            });
        g_api->Hide(g_views[i]);
    }
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
        KeyHandler::RegisterSink();
        for (int i = 0; i < 4; i++) {
            KeyHandler::GetSingleton()->Register(KEYS[i], KeyEventType::KEY_DOWN,
                [i]() { Toggle(i); });
        }
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api) CreateViews();
        break;
    }
}
```

---

## 6. Z-Ordering Two Overlapping Views

```cpp
// Background HUD — always visible, no focus, low z-order
g_hudView = g_api->CreateView("hud.html", nullptr);
g_api->SetOrder(g_hudView, 0);
g_api->Show(g_hudView);  // visible but unfocused

// Popup menu — appears on top of HUD when opened
g_menuView = g_api->CreateView("menu.html", OnMenuReady);
g_api->SetOrder(g_menuView, 10);
g_api->Hide(g_menuView);
```

The HUD renders continuously. When the menu is opened with `Show` + `Focus`, it renders over the HUD because its order value (10) is higher.

---

## 7. Updating a HUD Each Second

For a heads-up display that shows live stats (health, ammo, time), push updates from a recurring task rather than every frame.

```cpp
// In OnDomReady — schedule repeating updates via a recurring task
static void ScheduleHudUpdate() {
    F4SE::GetTaskInterface()->AddTask([]() {
        if (!g_api || !g_api->IsValid(g_hudView) || g_api->IsHidden(g_hudView)) {
            ScheduleHudUpdate();  // keep scheduling even when hidden
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) { ScheduleHudUpdate(); return; }

        std::string json = "{\"hp\":" + std::to_string((int)player->GetActorValue(RE::ActorValue::kHealth))
                         + ",\"ap\":" + std::to_string((int)player->GetActorValue(RE::ActorValue::kActionPoints))
                         + "}";
        g_api->InteropCall(g_hudView, "updateStats", json.c_str());

        // Re-schedule via timer (1000ms)
        // In practice, hook a game timer or use a separate thread
        ScheduleHudUpdate();
    });
}
```

For a real HUD, hook the game's tick or use a 1-second timer rather than a tight recursion loop. The example above shows the pattern; adjust the scheduling mechanism to your needs.

---

## 8. Inspector Setup (Development Only)

```cpp
#ifdef PRISMA_DEV

static void SetupInspector(PrismaView view) {
    api->CreateInspectorView(view);
    // Position inspector on right half of a 1920-wide screen
    api->SetInspectorBounds(view, 960.0f, 0.0f, 960, 600);
    api->SetInspectorVisibility(view, true);
}

// Bind F12 to toggle inspector during dev
KeyHandler::GetSingleton()->Register(0x58, KeyEventType::KEY_DOWN, []() {
    bool v = api->IsInspectorVisible(g_view);
    api->SetInspectorVisibility(g_view, !v);
});

#endif
```

Remove all `CreateInspectorView` calls before releasing your mod.

---

## 9. Error-Resilient View Creation

```cpp
static void CreateViews() {
    if (!g_api) {
        logger::error("PrismaUI not available — is PrismaUI_F4.dll installed and loaded?");
        return;
    }
    if (g_view != 0) return;  // already created

    try {
        g_view = g_api->CreateView("mymenu.html", OnDomReady);
    } catch (const std::exception& e) {
        logger::error("CreateView failed: {}", e.what());
        return;
    }

    if (!g_api->IsValid(g_view)) {
        logger::error("Created view is immediately invalid — check that mymenu.html exists in Data/PrismaUI_F4/views/");
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

The most common failure modes:
- `g_api` is null → PrismaUI_F4 is not installed or failed to load
- `IsValid` returns false immediately → the HTML file path is wrong
- JS errors in console → check HTML/JS syntax errors, missing function guards
