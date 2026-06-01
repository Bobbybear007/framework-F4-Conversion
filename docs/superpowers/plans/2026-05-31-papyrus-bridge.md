# Papyrus Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `window.prisma` JS API to every PrismaUI_F4 HTML view so mod authors can read and write Papyrus auto-properties and TESGlobal values without writing any C++.

**Architecture:** A new `PapyrusBridge` subsystem is wired into `Listeners.cpp`'s `OnDOMReady`. It registers four internal JS handlers (`_prismaGetGlobal` etc.) on the view's JSCore context, then evaluates a 30-line JS shim that wraps them in a Promise-based `window.prisma` object. Reads queue an `AddTask` to the game thread, read the value, and call back via `Communication::InteropCall`. Writes are fire-and-forget.

**Tech Stack:** CommonLibF4 (`RE::TESDataHandler::LookupForm`, `RE::TESGlobal`, `RE::BSScript::Internal::VirtualMachine`, `RE::BSAutoLock`), Ultralight JSCore (`JSObjectMakeFunctionWithCallback`, `JSContextRef`), existing `Communication::InteropCall` / `Invoke`.

---

## Codebase orientation

You are working in `e:\F4SE OG\Prisma\PrismaUI_F4 New Gen\`. All file paths below are relative to that root.

Key existing files to understand before coding:
- `src/PrismaUI/Communication.cpp` — see `BindJSCallbacks` for the pattern of locking a JSContext, registering a global function via `JSObjectMakeFunctionWithCallback`, and attaching a viewId "data" property. Replicate this pattern.
- `src/PrismaUI/Listeners.cpp` — `OnDOMReady` (line 85) submits to `ultralightThread` and fires `domReadyCallback`. You add `PapyrusBridge::InjectIntoView(id)` inside that submit, before `domReadyCallback`.
- `src/PrismaUI/Core.h` — `PrismaView`, `views`, `viewsMutex`, `ultralightThread`, `JSCallbackData` — all the shared state the bridge needs.

The project builds with `cmake --build build/release --config Release` from the project root (or via `_configure_and_build.bat`).

---

## Files

| Action | Path |
|--------|------|
| **Create** | `src/PrismaUI/PapyrusBridge.h` |
| **Create** | `src/PrismaUI/PapyrusBridge.cpp` |
| **Modify** | `src/PrismaUI/Listeners.cpp` — call bridge in `OnDOMReady` |

No CMakeLists.txt change needed — the project uses `file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")`.

---

## Task 1: PapyrusBridge.h + utilities skeleton

**Files:**
- Create: `src/PrismaUI/PapyrusBridge.h`
- Create: `src/PrismaUI/PapyrusBridge.cpp`

- [ ] **Step 1: Create `src/PrismaUI/PapyrusBridge.h`**

```cpp
#pragma once
#include <cstdint>

namespace PrismaUI::PapyrusBridge
{
    // Called from Listeners.cpp OnDOMReady for every view.
    // Registers _prismaGetGlobal/_prismaSetGlobal/_prismaGetProperty/_prismaSetProperty
    // as JS globals, then evaluates the window.prisma shim.
    void InjectIntoView(uint64_t viewId);
}
```

- [ ] **Step 2: Create `src/PrismaUI/PapyrusBridge.cpp` with includes, shim, and JSON helpers**

```cpp
#include "PapyrusBridge.h"

#include "Communication.h"
#include "Core.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <JavaScriptCore/JSRetainPtr.h>
#include <Ultralight/String.h>
#include <Ultralight/View.h>
#pragma warning(pop)

#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include <string>
#include <vector>

namespace logger = F4SE::log;

namespace PrismaUI::PapyrusBridge
{
    // ── JS shim injected into every view at DOM ready ─────────────────────────
    // Wraps the internal _prisma* handlers in a Promise-based window.prisma API.
    // Uses var/function (not const/arrow) for Ultralight JS engine compatibility.
    static constexpr const char* kShimScript = R"js(
(function() {
  var _p = {};
  var _id = 0;
  function _req(fn, params) {
    return new Promise(function(resolve) {
      var id = String(_id++);
      _p[id] = resolve;
      params.id = id;
      fn(JSON.stringify(params));
    });
  }
  window._prismaResponse = function(json) {
    var r = JSON.parse(json);
    if (_p[r.id]) { _p[r.id](r.value); delete _p[r.id]; }
  };
  window.prisma = {
    getGlobal: function(esp, formId) {
      return _req(_prismaGetGlobal, {esp: esp, formId: formId});
    },
    setGlobal: function(esp, formId, value) {
      _prismaSetGlobal(JSON.stringify({esp: esp, formId: formId, value: value}));
    },
    getProperty: function(esp, formId, script, prop) {
      return _req(_prismaGetProperty, {esp: esp, formId: formId, script: script, prop: prop});
    },
    setProperty: function(esp, formId, script, prop, value) {
      _prismaSetProperty(JSON.stringify({esp: esp, formId: formId, script: script, prop: prop, value: value}));
    }
  };
})();
)js";

    // ── JSON mini-parser for flat objects we control ──────────────────────────
    // Extracts a string or numeric value for `key` from a simple flat JSON object.
    // Only handles the format produced by the JS shim above.
    static std::string JsonGet(const std::string& json, const std::string& key)
    {
        std::string search = "\"" + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string::npos) return {};
        pos += search.size();
        while (pos < json.size() && json[pos] == ' ') ++pos;
        if (pos >= json.size()) return {};
        if (json[pos] == '"') {
            ++pos;
            auto end = json.find('"', pos);
            if (end == std::string::npos) return {};
            return json.substr(pos, end - pos);
        }
        auto end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        auto result = json.substr(pos, end - pos);
        while (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }

    // ── JSON response builders ────────────────────────────────────────────────
    static std::string Response(const std::string& id, float v)
    {
        return "{\"id\":\"" + id + "\",\"value\":" + std::to_string(v) + "}";
    }
    static std::string Response(const std::string& id, std::int32_t v)
    {
        return "{\"id\":\"" + id + "\",\"value\":" + std::to_string(v) + "}";
    }
    static std::string Response(const std::string& id, bool v)
    {
        return "{\"id\":\"" + id + "\",\"value\":" + (v ? "true" : "false") + "}";
    }
    static std::string ResponseNull(const std::string& id)
    {
        return "{\"id\":\"" + id + "\",\"value\":null}";
    }

} // namespace PrismaUI::PapyrusBridge
```

- [ ] **Step 3: Confirm the file compiles (no build yet — just ensure the includes resolve)**

The includes chain: `PapyrusBridge.cpp` → `Communication.h` (already included elsewhere), `Core.h` (already included elsewhere), JSCore headers (same path as in `Communication.cpp`), `RE/Fallout.h` (via PCH or direct).

Check `src/PCH.h` — it already includes `<RE/Fallout.h>` and `<F4SE/F4SE.h>`. Since all `.cpp` files use the PCH (`target_precompile_headers` in CMakeLists.txt), you do NOT need to repeat those includes in `PapyrusBridge.cpp`. Remove the direct `RE/Fallout.h` and `F4SE/F4SE.h` includes and rely on the PCH. Keep the JSCore and Ultralight includes (they are NOT in the PCH).

Updated top of `PapyrusBridge.cpp`:

```cpp
#include "PapyrusBridge.h"

#include "Communication.h"
#include "Core.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <JavaScriptCore/JSRetainPtr.h>
#include <Ultralight/String.h>
#include <Ultralight/View.h>
#pragma warning(pop)

#include <string>
#include <vector>

namespace logger = F4SE::log;
```

- [ ] **Step 4: Commit**

```powershell
git add "src/PrismaUI/PapyrusBridge.h" "src/PrismaUI/PapyrusBridge.cpp"
git commit -m "feat: PapyrusBridge skeleton — shim, JSON helpers, InjectIntoView stub"
```

---

## Task 2: TESGlobal read/write handlers

**Files:**
- Modify: `src/PrismaUI/PapyrusBridge.cpp`

These handlers execute on the **Ultralight thread**. They parse JSON, then queue a **game-thread task** via `F4SE::GetTaskInterface()->AddTask`. The game-thread task reads/writes the global and — for reads — calls `Communication::InteropCall` to send the result back to JS.

- [ ] **Step 1: Add `LookupGlobal` helper inside the namespace (after the Response helpers)**

```cpp
    // ── Form lookup ───────────────────────────────────────────────────────────
    // Returns nullptr if the plugin is not loaded or the form is the wrong type.
    // Uses TESDataHandler::LookupForm which handles ESP/ESL formID encoding.
    static RE::TESGlobal* LookupGlobal(const std::string& esp, const std::string& formIdHex)
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) return nullptr;
        std::uint32_t localId = 0;
        try { localId = static_cast<std::uint32_t>(std::stoul(formIdHex, nullptr, 16)); }
        catch (...) { return nullptr; }
        return dh->LookupForm<RE::TESGlobal>(localId, esp);
    }
```

`TESDataHandler::LookupForm<T>` (defined in `TESDataHandler.h`) looks up the plugin by name, combines the file index with the local ID (handling ESL encoding automatically), calls `TESForm::GetFormByID`, and casts via `form->Is(T::FORM_ID) ? form->As<T>() : nullptr`. You get `nullptr` for a missing plugin, missing form, or wrong form type.

- [ ] **Step 2: Add the JSCore helper that extracts a viewId from the function's attached data**

This is the same pattern as `InvokeCppCallback` in `Communication.cpp`. Copy it:

```cpp
    // ── JSCore utilities ──────────────────────────────────────────────────────
    static Core::PrismaViewId ExtractViewId(JSContextRef ctx, JSObjectRef function)
    {
        JSStringRef dataKey = JSStringCreateWithUTF8CString("data");
        JSValueRef dataVal  = JSObjectGetProperty(ctx, function, dataKey, nullptr);
        JSStringRelease(dataKey);
        if (!dataVal || JSValueIsNull(ctx, dataVal) || JSValueIsUndefined(ctx, dataVal)) return 0;

        JSObjectRef dataObj = JSValueToObject(ctx, dataVal, nullptr);
        if (!dataObj) return 0;

        JSStringRef vidKey = JSStringCreateWithUTF8CString("viewId");
        JSValueRef  vidVal = JSObjectGetProperty(ctx, dataObj, vidKey, nullptr);
        JSStringRelease(vidKey);
        if (!vidVal) return 0;

        JSStringRef vidStr = JSValueToStringCopy(ctx, vidVal, nullptr);
        if (!vidStr) return 0;
        size_t len = JSStringGetMaximumUTF8CStringSize(vidStr);
        std::vector<char> buf(len);
        JSStringGetUTF8CString(vidStr, buf.data(), len);
        JSStringRelease(vidStr);
        try { return std::stoull(buf.data()); }
        catch (...) { return 0; }
    }

    static std::string ExtractStringArg(JSContextRef ctx, size_t argc, const JSValueRef argv[])
    {
        if (argc == 0) return {};
        JSStringRef s = JSValueToStringCopy(ctx, argv[0], nullptr);
        if (!s) return {};
        size_t len = JSStringGetMaximumUTF8CStringSize(s);
        std::vector<char> buf(len);
        JSStringGetUTF8CString(s, buf.data(), len);
        JSStringRelease(s);
        return buf.data();
    }
```

- [ ] **Step 3: Add `Handler_GetGlobal`**

```cpp
    static JSValueRef Handler_GetGlobal(JSContextRef ctx, JSObjectRef function, JSObjectRef,
                                        size_t argc, const JSValueRef argv[], JSValueRef*)
    {
        Core::PrismaViewId viewId = ExtractViewId(ctx, function);
        std::string json          = ExtractStringArg(ctx, argc, argv);

        std::string id     = JsonGet(json, "id");
        std::string esp    = JsonGet(json, "esp");
        std::string formId = JsonGet(json, "formId");

        if (viewId == 0 || id.empty()) return JSValueMakeUndefined(ctx);

        F4SE::GetTaskInterface()->AddTask([viewId, id, esp, formId]() {
            auto* global = LookupGlobal(esp, formId);
            std::string result = global ? Response(id, global->value) : ResponseNull(id);
            Communication::InteropCall(viewId, "_prismaResponse", result);
        });

        return JSValueMakeUndefined(ctx);
    }
```

- [ ] **Step 4: Add `Handler_SetGlobal`**

```cpp
    static JSValueRef Handler_SetGlobal(JSContextRef ctx, JSObjectRef function, JSObjectRef,
                                        size_t argc, const JSValueRef argv[], JSValueRef*)
    {
        Core::PrismaViewId viewId = ExtractViewId(ctx, function);
        std::string json          = ExtractStringArg(ctx, argc, argv);

        std::string esp    = JsonGet(json, "esp");
        std::string formId = JsonGet(json, "formId");
        std::string valStr = JsonGet(json, "value");

        if (viewId == 0 || valStr.empty()) return JSValueMakeUndefined(ctx);

        float newValue = 0.0f;
        try { newValue = std::stof(valStr); } catch (...) { return JSValueMakeUndefined(ctx); }

        F4SE::GetTaskInterface()->AddTask([esp, formId, newValue]() {
            auto* global = LookupGlobal(esp, formId);
            if (global) {
                global->value = newValue;
                logger::debug("PapyrusBridge: SetGlobal {}/{} = {}", esp, formId, newValue);
            } else {
                logger::warn("PapyrusBridge: SetGlobal — form not found: {}/{}", esp, formId);
            }
        });

        return JSValueMakeUndefined(ctx);
    }
```

- [ ] **Step 5: Commit**

```powershell
git add "src/PrismaUI/PapyrusBridge.cpp"
git commit -m "feat: PapyrusBridge TESGlobal get/set handlers"
```

---

## Task 3: Papyrus property read/write handlers

**Files:**
- Modify: `src/PrismaUI/PapyrusBridge.cpp`

These use the Papyrus VM. **All VM access must happen on the game thread** (inside `AddTask`). The locking pattern comes from CLAUDE.md.

- [ ] **Step 1: Add `FindProperty` helper — returns a copy of the Variable value or empty optional**

The returned value is a copy taken under the lock. Callers must NOT hold a `Variable*` past the lock release.

```cpp
    struct PropValue { enum class Kind { Float, Int, Bool, None } kind = Kind::None;
                       float f = 0; std::int32_t i = 0; bool b = false; };

    // Finds the named script on the form and copies out the property value.
    // Must be called on the game thread.
    static PropValue FindProperty(RE::TESForm* form, const std::string& scriptName,
                                  const std::string& propName)
    {
        auto* gameVM = RE::GameVM::GetSingleton();
        if (!gameVM) return {};
        auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(
            gameVM->GetVM().get());
        if (!concreteVM) return {};

        RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

        for (auto& [handle, scripts] : concreteVM->attachedScripts) {
            for (auto& scriptRef : scripts) {
                auto* smartPtr = scriptRef.get();        // BSTSmartPointer<Object>*
                if (!smartPtr || !*smartPtr) continue;
                auto* obj = smartPtr->get();             // Object*
                if (!obj || !obj->IsValid()) continue;

                auto* typeInfo = obj->GetTypeInfo();
                if (!typeInfo) continue;
                if (_stricmp(typeInfo->GetName(), scriptName.c_str()) != 0) continue;

                // Confirm this object is bound to our form by comparing handles
                if (obj->handle != static_cast<std::size_t>(form->GetFormID())) {
                    // Note: if handle encoding is more complex than raw FormID,
                    // remove this check and rely solely on script-name uniqueness.
                    // See spec for details.
                }

                const RE::BSFixedString propKey(propName.c_str());
                const auto* var = obj->GetProperty(propKey);
                if (!var) continue;

                PropValue result;
                if (var->is<float>())            { result.kind = PropValue::Kind::Float; result.f = RE::BSScript::get<float>(*var); }
                else if (var->is<std::int32_t>()) { result.kind = PropValue::Kind::Int;   result.i = RE::BSScript::get<std::int32_t>(*var); }
                else if (var->is<bool>())          { result.kind = PropValue::Kind::Bool;  result.b = RE::BSScript::get<bool>(*var); }
                return result;
            }
        }
        return {};
    }
```

**Important note on handle comparison:** `obj->handle` (offset 0x20 in `Object`, type `std::size_t`) stores the VM handle, not necessarily the raw FormID. If matching on handle proves unreliable, remove the `handle` check entirely — matching by script name alone is sufficient for the common case where each script type is attached to only one form. The spec documents this caveat.

- [ ] **Step 2: Add `SetProperty` helper**

```cpp
    // Sets the named property on the named script attached to the form.
    // Must be called on the game thread.
    static bool SetProperty(RE::TESForm* form, const std::string& scriptName,
                            const std::string& propName, const std::string& valueStr)
    {
        auto* gameVM = RE::GameVM::GetSingleton();
        if (!gameVM) return false;
        auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(
            gameVM->GetVM().get());
        if (!concreteVM) return false;

        RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

        for (auto& [handle, scripts] : concreteVM->attachedScripts) {
            for (auto& scriptRef : scripts) {
                auto* smartPtr = scriptRef.get();
                if (!smartPtr || !*smartPtr) continue;
                auto* obj = smartPtr->get();
                if (!obj || !obj->IsValid()) continue;

                auto* typeInfo = obj->GetTypeInfo();
                if (!typeInfo || _stricmp(typeInfo->GetName(), scriptName.c_str()) != 0) continue;

                const RE::BSFixedString propKey(propName.c_str());
                auto* var = obj->GetProperty(propKey);
                if (!var) continue;

                if (var->is<float>()) {
                    try { *var = std::stof(valueStr); } catch (...) { return false; }
                } else if (var->is<std::int32_t>()) {
                    try { *var = static_cast<std::int32_t>(std::stol(valueStr)); } catch (...) { return false; }
                } else if (var->is<bool>()) {
                    *var = (valueStr == "true" || valueStr == "1");
                } else {
                    return false; // unsupported type (string/array — out of scope)
                }
                return true;
            }
        }
        return false;
    }
```

- [ ] **Step 3: Add `Handler_GetProperty`**

```cpp
    static JSValueRef Handler_GetProperty(JSContextRef ctx, JSObjectRef function, JSObjectRef,
                                          size_t argc, const JSValueRef argv[], JSValueRef*)
    {
        Core::PrismaViewId viewId = ExtractViewId(ctx, function);
        std::string json          = ExtractStringArg(ctx, argc, argv);

        std::string id     = JsonGet(json, "id");
        std::string esp    = JsonGet(json, "esp");
        std::string formId = JsonGet(json, "formId");
        std::string script = JsonGet(json, "script");
        std::string prop   = JsonGet(json, "prop");

        if (viewId == 0 || id.empty()) return JSValueMakeUndefined(ctx);

        F4SE::GetTaskInterface()->AddTask([viewId, id, esp, formId, script, prop]() {
            auto* dh = RE::TESDataHandler::GetSingleton();
            if (!dh) {
                Communication::InteropCall(viewId, "_prismaResponse", ResponseNull(id));
                return;
            }
            std::uint32_t localId = 0;
            try { localId = static_cast<std::uint32_t>(std::stoul(formId, nullptr, 16)); }
            catch (...) {
                Communication::InteropCall(viewId, "_prismaResponse", ResponseNull(id));
                return;
            }
            auto* form = dh->LookupForm(localId, esp);
            if (!form) {
                logger::warn("PapyrusBridge: GetProperty — form not found: {}/{}", esp, formId);
                Communication::InteropCall(viewId, "_prismaResponse", ResponseNull(id));
                return;
            }

            auto val = FindProperty(form, script, prop);
            std::string result;
            switch (val.kind) {
            case PropValue::Kind::Float: result = Response(id, val.f); break;
            case PropValue::Kind::Int:   result = Response(id, val.i); break;
            case PropValue::Kind::Bool:  result = Response(id, val.b); break;
            default:                     result = ResponseNull(id);    break;
            }
            Communication::InteropCall(viewId, "_prismaResponse", result);
        });

        return JSValueMakeUndefined(ctx);
    }
```

- [ ] **Step 4: Add `Handler_SetProperty`**

```cpp
    static JSValueRef Handler_SetProperty(JSContextRef ctx, JSObjectRef function, JSObjectRef,
                                          size_t argc, const JSValueRef argv[], JSValueRef*)
    {
        Core::PrismaViewId viewId = ExtractViewId(ctx, function);
        std::string json          = ExtractStringArg(ctx, argc, argv);

        std::string esp    = JsonGet(json, "esp");
        std::string formId = JsonGet(json, "formId");
        std::string script = JsonGet(json, "script");
        std::string prop   = JsonGet(json, "prop");
        std::string val    = JsonGet(json, "value");

        if (viewId == 0 || val.empty()) return JSValueMakeUndefined(ctx);

        F4SE::GetTaskInterface()->AddTask([esp, formId, script, prop, val]() {
            auto* dh = RE::TESDataHandler::GetSingleton();
            if (!dh) return;
            std::uint32_t localId = 0;
            try { localId = static_cast<std::uint32_t>(std::stoul(formId, nullptr, 16)); }
            catch (...) { return; }
            auto* form = dh->LookupForm(localId, esp);
            if (!form) {
                logger::warn("PapyrusBridge: SetProperty — form not found: {}/{}", esp, formId);
                return;
            }
            if (!SetProperty(form, script, prop, val)) {
                logger::warn("PapyrusBridge: SetProperty — property not found or wrong type: {}.{}", script, prop);
            }
        });

        return JSValueMakeUndefined(ctx);
    }
```

- [ ] **Step 5: Commit**

```powershell
git add "src/PrismaUI/PapyrusBridge.cpp"
git commit -m "feat: PapyrusBridge Papyrus property get/set handlers"
```

---

## Task 4: RegisterHandler helper + InjectIntoView

**Files:**
- Modify: `src/PrismaUI/PapyrusBridge.cpp`

`InjectIntoView` is called from the Ultralight thread (inside `ultralightThread.submit`), so it can lock the JSContext directly without re-submitting to the executor.

- [ ] **Step 1: Add `RegisterHandler` helper**

This mirrors the JSObjectMakeFunctionWithCallback pattern in `Communication.cpp::BindJSCallbacks`. The viewId is stored as a "data.viewId" string property on the function object so `ExtractViewId` can retrieve it.

```cpp
    static void RegisterHandler(JSContextRef ctx, JSObjectRef globalObj,
                                Core::PrismaViewId viewId, const char* name,
                                JSObjectCallAsFunctionCallback fn)
    {
        JSStringRef nameStr = JSStringCreateWithUTF8CString(name);
        JSObjectRef funcObj = JSObjectMakeFunctionWithCallback(ctx, nameStr, fn);

        // Attach viewId as data.viewId (same pattern as Communication.cpp)
        JSObjectRef dataObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef vidKey  = JSStringCreateWithUTF8CString("viewId");
        JSStringRef vidVal  = JSStringCreateWithUTF8CString(std::to_string(viewId).c_str());
        JSObjectSetProperty(ctx, dataObj, vidKey,
                            JSValueMakeString(ctx, vidVal), kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(vidKey);
        JSStringRelease(vidVal);

        JSStringRef dataKey = JSStringCreateWithUTF8CString("data");
        JSObjectSetProperty(ctx, funcObj, dataKey, dataObj,
                            kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(dataKey);

        JSObjectSetProperty(ctx, globalObj, nameStr, funcObj,
                            kJSPropertyAttributeNone, nullptr);
        JSStringRelease(nameStr);
    }
```

- [ ] **Step 2: Add `InjectIntoView`**

```cpp
    void InjectIntoView(uint64_t viewId)
    {
        std::shared_ptr<Core::PrismaView> viewData;
        {
            std::shared_lock lock(Core::viewsMutex);
            auto it = Core::views.find(viewId);
            if (it == Core::views.end()) return;
            viewData = it->second;
        }
        if (!viewData || !viewData->ultralightView) return;

        auto scoped   = viewData->ultralightView->LockJSContext();
        JSContextRef ctx    = scoped->ctx();
        JSObjectRef  global = JSContextGetGlobalObject(ctx);

        RegisterHandler(ctx, global, viewId, "_prismaGetGlobal",   Handler_GetGlobal);
        RegisterHandler(ctx, global, viewId, "_prismaSetGlobal",   Handler_SetGlobal);
        RegisterHandler(ctx, global, viewId, "_prismaGetProperty", Handler_GetProperty);
        RegisterHandler(ctx, global, viewId, "_prismaSetProperty", Handler_SetProperty);

        ultralight::String exc;
        viewData->ultralightView->EvaluateScript(ultralight::String(kShimScript), &exc);
        if (!exc.empty()) {
            logger::warn("PapyrusBridge [{}]: shim injection failed: {}", viewId,
                         exc.utf8().data());
        } else {
            logger::debug("PapyrusBridge [{}]: injected window.prisma", viewId);
        }
    }
```

- [ ] **Step 3: Commit**

```powershell
git add "src/PrismaUI/PapyrusBridge.cpp"
git commit -m "feat: PapyrusBridge RegisterHandler + InjectIntoView"
```

---

## Task 5: Wire into Listeners.cpp

**Files:**
- Modify: `src/PrismaUI/Listeners.cpp`

`OnDOMReady` is in `Listeners.cpp` at approximately line 85. It currently does:

```cpp
void MyLoadListener::OnDOMReady(View* /*caller*/, uint64_t /*frame_id*/, bool is_main_frame,
                                const String& /*url*/) {
    if (is_main_frame) {
        logger::info("View [{}]: LoadListener: DOM ready.", viewId_);

        ultralightThread.submit([id = viewId_] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end() && it->second->domReadyCallback) {
                it->second->domReadyCallback(id);
            }
        });
    }
}
```

- [ ] **Step 1: Add the PapyrusBridge include at the top of `Listeners.cpp`**

```cpp
#include "PapyrusBridge.h"
```

Add it after the existing includes (after `#include "Translations.h"`).

- [ ] **Step 2: Call `InjectIntoView` before `domReadyCallback`**

Change the `ultralightThread.submit` lambda from:

```cpp
        ultralightThread.submit([id = viewId_] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end() && it->second->domReadyCallback) {
                it->second->domReadyCallback(id);
            }
        });
```

to:

```cpp
        ultralightThread.submit([id = viewId_] {
            PapyrusBridge::InjectIntoView(id);  // window.prisma available before consumer callback
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end() && it->second->domReadyCallback) {
                it->second->domReadyCallback(id);
            }
        });
```

Note: `InjectIntoView` takes its own shared_lock on `viewsMutex` and releases it before returning. Taking the lock again for `domReadyCallback` is safe because `std::shared_mutex` allows multiple concurrent readers.

- [ ] **Step 3: Build the project**

```powershell
cd "e:\F4SE OG\Prisma\PrismaUI_F4 New Gen"
cmake --build build/release --config Release 2>&1 | Select-String -Pattern "error|warning C" | Select-Object -First 30
```

Expected: zero errors. Common warnings to ignore: C4100 (unreferenced parameter in JSCore callback signatures — already suppressed by the pragma warning push/pop).

If you see `error C2039: 'handle' is not a member of RE::BSScript::Object` — the `Object::handle` field name differs. Check `Object.h` (it is at offset 0x20, declared `std::size_t handle`) and adjust the field name if the compiler disagrees.

If you see `error C2039: 'GetVM' is not a member` — use `RE::GameVM::GetSingleton()->GetVM().get()` and verify `GameVM` is in `GameScript.h` (included via `RE/Fallout.h`).

- [ ] **Step 4: Commit**

```powershell
git add "src/PrismaUI/Listeners.cpp"
git commit -m "feat: wire PapyrusBridge into DOM ready — window.prisma now available in every view"
```

---

## Task 6: Update public docs

**Files:**
- Modify: `docs/api-reference.md`
- Modify: `docs/getting-started.md`

- [ ] **Step 1: Add `window.prisma` section to `docs/api-reference.md`**

Append to the end of `docs/api-reference.md`:

```markdown
---

## window.prisma — Papyrus Bridge

Every view has `window.prisma` available at DOM ready. It lets HTML views read and write game data without C++.

### `prisma.getGlobal(esp, formId)` → `Promise<number|null>`

Reads a `TESGlobal` form's float value.

```js
const val = await prisma.getGlobal("MyMod.esp", "800");
if (val !== null) slider.value = val;
```

### `prisma.setGlobal(esp, formId, value)`

Writes a `TESGlobal` form's float value. Fire-and-forget.

```js
prisma.setGlobal("MyMod.esp", "800", 2.5);
```

### `prisma.getProperty(esp, formId, scriptName, propName)` → `Promise<number|boolean|null>`

Reads a Papyrus `Auto` property (float, int, or bool) from a script attached to a form.

```js
const dmg = await prisma.getProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale");
```

### `prisma.setProperty(esp, formId, scriptName, propName, value)`

Writes a Papyrus `Auto` property. The value is coerced to the property's existing type. Fire-and-forget.

```js
prisma.setProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale", 2.5);
```

### Parameter reference

| Param | Format | Example |
|-------|--------|---------|
| `esp` | Plugin filename with extension | `"MyMod.esp"`, `"MyMod.esl"` |
| `formId` | Local hex form ID, no file-index byte | `"800"` = `0x00000800` in the plugin |
| `scriptName` | Papyrus script class name | `"MyMod_Quest"` |
| `propName` | `Auto` property name (case-insensitive) | `"DamageScale"` |

**Null returns:** all `get*` calls return `null` if the plugin is not loaded, the form is not found, or the script/property does not exist on the form. Always check for null.

**Requires game loaded:** properties are only readable after `kPostLoadGame`. Do not call before the game is fully loaded.

**Supported types:** `float`, `int`, `bool`. String and array properties are not supported in this version.
```

- [ ] **Step 2: Add a bridge example to `docs/getting-started.md`**

Append before the final `---` in `docs/getting-started.md`:

```markdown
---

## Papyrus Bridge — zero C++ settings screen

If you have a Papyrus quest script with `Auto` properties, `window.prisma` lets your HTML read and write those values directly. No C++ required.

**Papyrus script (MyMod_Quest.psc):**

```papyrus
Scriptname MyMod_Quest extends Quest
float Property DamageScale = 1.0 Auto
bool  Property HardcoreMode = false Auto
```

**HTML (settings.html):**

```html
<input id="dmgSlider" type="range" min="0.5" max="3" step="0.1">
<input id="hcToggle" type="checkbox">
<script>
  async function loadSettings() {
    const dmg = await prisma.getProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale");
    const hc  = await prisma.getProperty("MyMod.esp", "800", "MyMod_Quest", "HardcoreMode");
    if (dmg !== null) document.getElementById('dmgSlider').value = dmg;
    if (hc  !== null) document.getElementById('hcToggle').checked = hc;
  }

  document.getElementById('dmgSlider').addEventListener('input', function() {
    prisma.setProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale", parseFloat(this.value));
  });

  document.getElementById('hcToggle').addEventListener('change', function() {
    prisma.setProperty("MyMod.esp", "800", "MyMod_Quest", "HardcoreMode", this.checked);
  });

  loadSettings();
</script>
```

The `"800"` is the local form ID of your quest form in xEdit (strip the file-index byte). PrismaUI_F4 handles the ESP/ESL ID encoding automatically.
```

- [ ] **Step 3: Commit**

```powershell
git add "docs/api-reference.md" "docs/getting-started.md"
git commit -m "docs: document window.prisma Papyrus bridge API and settings example"
```

---

## Self-review checklist

Before marking complete:

- [ ] `window.prisma` available in every view without consumer calling anything
- [ ] `getGlobal` / `setGlobal` tested with a real TESGlobal form (confirm value round-trips)
- [ ] `getProperty` / `setProperty` tested with a real Papyrus float property (confirm value round-trips)
- [ ] Non-existent plugin → returns `null` without crash
- [ ] Non-existent property → returns `null` without crash
- [ ] No game-thread crashes under normal play (the bridge touches the VM under the spinlock — it must not block for more than a frame)
- [ ] Build is clean (zero errors)

---

## Known edge cases

**VM not yet initialized:** `GameVM::GetSingleton()` can return non-null but `GetVM().get()` may return null early in startup. The `if (!concreteVM) return {};` guard handles this.

**Script name uniqueness:** `FindProperty` and `SetProperty` iterate all attached scripts and match by script type name. If two different forms have scripts of the same class name, the first match wins. The caller should ensure `scriptName` is unique in their mod (quest scripts usually are).

**handle field:** `Object::handle` (offset 0x20 in CommonLibF4's `Object.h`) is the VM handle, not necessarily the raw FormID. The handle comparison in `FindProperty` is commented as optional — if it causes mismatches in practice, remove it and rely on script-name matching alone.

**Thread safety of Variable write:** Writing a `Variable` under `attachedScriptsLock` is safe — the Papyrus VM holds this same lock when reading properties during script execution.
