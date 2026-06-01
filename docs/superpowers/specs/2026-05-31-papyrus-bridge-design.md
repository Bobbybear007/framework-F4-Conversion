# Papyrus Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `window.prisma` JS API to PrismaUI_F4 core that lets any HTML view read and write Papyrus auto-properties and TESGlobal values without requiring any C++ code from the mod author.

**Architecture:** A new `PapyrusBridge` subsystem registers four internal JS-callable C++ handlers on every view at DOM ready, then injects a small JS shim that wraps them in a clean Promise-based `window.prisma` object. Reads are async (game-thread task → InteropCall response). Writes are fire-and-forget. No new public API surface — this is transparent infrastructure.

**Tech Stack:** CommonLibF4 (RE::TESDataHandler, RE::TESGlobal, RE::BSScript::Internal::VirtualMachine), Ultralight JSCore (JSObjectMakeFunctionWithCallback, JSObjectSetProperty), existing PrismaUI Communication::InteropCall / Communication::Invoke.

---

## JS API (what mod authors write)

```js
// TESGlobal — read/write a global form's float value
const val = await prisma.getGlobal("MyMod.esp", "800");
prisma.setGlobal("MyMod.esp", "800", 3.0);

// Papyrus auto-property — read/write float/int/bool on a script attached to a form
const dmg = await prisma.getProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale");
prisma.setProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale", 2.5);
```

- `esp` — plugin filename including extension, e.g. `"MyMod.esp"` or `"MyMod.esl"`
- `formId` — local hex form ID as a string, without the file index byte: `"800"` = `0x00000800` in the plugin
- `scriptName` — exact name of the Papyrus script attached to the form (case-insensitive match is fine)
- `propName` — exact name of the `Auto` property declared on that script (case-insensitive)
- Returns are Promises for reads, `undefined` for writes

---

## Files

| Action | Path |
|--------|------|
| **Create** | `src/PrismaUI/PapyrusBridge.h` |
| **Create** | `src/PrismaUI/PapyrusBridge.cpp` |
| **Modify** | `src/PrismaUI/Listeners.cpp` — call `PapyrusBridge::InjectIntoView(viewId)` at DOM ready |
| **Modify** | `src/PrismaUI/Core.h` — include PapyrusBridge.h |

---

## Threading model

```
JS call (_prismaGetGlobal)       ← Ultralight thread
  → extract JSON params
  → AddTask(game thread task)

Game thread task:
  → resolve form + read value
  → Communication::InteropCall(viewId, "_prismaResponse", json)
    → Ultralight thread: calls window._prismaResponse(json)
      → shim resolves Promise
```

Writes skip the response step — the game thread task just writes and returns.

**Never** access RE:: game data on the Ultralight thread. All RE:: calls happen inside `AddTask` lambdas.

---

## Form lookup

```cpp
// PapyrusBridge.cpp
static RE::TESForm* LookupForm(const std::string& esp, const std::string& formIdHex)
{
    auto* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) return nullptr;

    RE::TESFile* file = nullptr;
    for (auto* f : dh->files) {
        if (f && _stricmp(f->filename, esp.c_str()) == 0) {
            file = f;
            break;
        }
    }
    if (!file) return nullptr;

    RE::FormID localId = static_cast<RE::FormID>(std::stoul(formIdHex, nullptr, 16));

    // ESL files use 0xFE prefix + 12-bit index + 12-bit local ID
    RE::FormID resolvedId;
    // kSmallFile is the ESL flag — verify name in CommonLibF4's TESFile::RecordFlag enum;
    // it may be kLight or kSmall depending on the CLib version.
    if (file->flags.all(RE::TESFile::RecordFlag::kSmallFile)) {
        resolvedId = 0xFE000000 | (static_cast<RE::FormID>(file->smallFileCompileIndex) << 12) | (localId & 0xFFF);
    } else {
        resolvedId = (static_cast<RE::FormID>(file->compileIndex) << 24) | (localId & 0x00FFFFFF);
    }

    return RE::TESForm::LookupByID(resolvedId);
}
```

---

## TESGlobal read/write

```cpp
// Read
RE::TESForm* form = LookupForm(esp, formIdHex);
auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
float value = global ? global->value : 0.0f;

// Write
if (global) global->value = newValue;
```

---

## Papyrus property read/write

```cpp
// Access pattern from CLAUDE.md — must hold attachedScriptsLock
auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(
    RE::GameVM::GetSingleton()->GetVM().get());
RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

// Key is the VM handle for this form.
// RE::VirtualMachine::GetHandle(form, handle) writes the handle into `handle`.
// If GetHandle is unavailable, fall back to iterating all entries and matching
// the script's object's owner form — verify the exact key type against
// CommonLibF4's BSScript::Internal::VirtualMachine definition before coding.
uint64_t handle = 0;
concreteVM->GetHandle(form, RE::BSScript::GameObject::RTTI, handle);
auto it = concreteVM->attachedScripts.find(handle);
if (it == concreteVM->attachedScripts.end()) return; // no scripts on this form

for (auto& scriptRef : it->second) {
    auto* obj = scriptRef.get();
    if (!obj) continue;
    // Match script name (case-insensitive)
    if (_stricmp(obj->GetTypeInfo()->GetName(), scriptName.c_str()) != 0) continue;

    RE::BSScript::Variable* var = obj->GetProperty(RE::BSFixedString(propName.c_str()));
    if (!var) continue;

    // Read — return as JSON value based on type
    // Write — coerce to existing variable type
}
```

**Type handling for reads:** return a JSON object `{"type":"float","value":1.5}` or `{"type":"int","value":3}` or `{"type":"bool","value":true}`. The JS shim unwraps this — `getProperty` resolves with the raw JS value (number or bool), not the wrapper object.

**Type handling for writes:** read the existing variable's `GetType()` first. Coerce the incoming JS number to `float`, `int32_t`, or `bool` accordingly before calling `var->SetFloat` / `var->SetSInt` / `var->SetBool`.

---

## JS shim (injected via Invoke at DOM ready)

```js
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
```

Uses `var` and function expressions (not `const`/arrow functions) for compatibility with Ultralight's JS engine.

---

## PapyrusBridge.h

```cpp
#pragma once

namespace PrismaUI
{
    namespace PapyrusBridge
    {
        // Called at DOM ready for every view — registers _prisma* handlers and injects shim.
        void InjectIntoView(uint64_t viewId);
    }
}
```

---

## PapyrusBridge.cpp structure

```
PapyrusBridge.cpp
├── static std::string kShimScript    // the JS shim as a string literal
├── static RE::TESForm* LookupForm(esp, formIdHex)
├── static JSValueRef Handler_GetGlobal(ctx, fn, this, argc, argv, exc)
│     extracts {id, esp, formId} from argv[0]
│     AddTask → reads TESGlobal → InteropCall "_prismaResponse" {id, value}
├── static JSValueRef Handler_SetGlobal(ctx, fn, this, argc, argv, exc)
│     extracts {esp, formId, value}
│     AddTask → writes TESGlobal
├── static JSValueRef Handler_GetProperty(ctx, fn, this, argc, argv, exc)
│     extracts {id, esp, formId, script, prop}
│     AddTask → reads Papyrus variable → InteropCall "_prismaResponse" {id, value}
├── static JSValueRef Handler_SetProperty(ctx, fn, this, argc, argv, exc)
│     extracts {esp, formId, script, prop, value}
│     AddTask → writes Papyrus variable
└── void InjectIntoView(viewId)
      gets view's JSContext via LockJSContext
      registers four handlers as global functions via JSObjectMakeFunctionWithCallback
      calls Communication::Invoke(viewId, kShimScript)
```

The four `Handler_*` functions follow the same JSCore function callback signature as the existing `InvokeCppCallback` in Communication.cpp — copy that pattern.

---

## Listeners.cpp change

In the DOM ready dispatch (where `OnDomReadyCallback` is called for a view), add **before** calling the consumer's callback:

```cpp
PapyrusBridge::InjectIntoView(viewId);
```

This guarantees `window.prisma` exists by the time the plugin author's `OnDomReadyCallback` fires and they call `RegisterJSListener` or issue their first interop call.

---

## Error handling

- Plugin not loaded → `getGlobal`/`getProperty` resolve with `null`
- Form not found → resolves with `null`
- Form exists but is wrong type (not TESGlobal) → resolves with `null`
- Script not attached to form → resolves with `null`
- Property not found on script → resolves with `null`
- All errors logged via `logger::warn` with context (esp, formId, etc.)
- JS side: callers should check `if (val === null)`

---

## Limitations (by design)

- **Pull only** — there is no push/subscribe. If Papyrus changes a property, the HTML won't know unless it polls. Polling every second with `setInterval` is fine for settings screens.
- **No Papyrus function calls** — `prisma.callFunction(...)` is not in scope for this implementation.
- **No string properties** — `BSScript::Variable` string handling is complex. float/int/bool only in v1.
- **No array properties** — out of scope.
- **Requires script to be attached and instantiated** — properties are only readable after the game has loaded and the form's script has been initialized by the Papyrus VM. Always use after `kPostLoadGame`.

---

## Checklist before in-game test

- [ ] Plugin with a quest form and at least one `Auto` float property compiled and loaded
- [ ] TESGlobal form in the same plugin
- [ ] HTML calls `await prisma.getGlobal(...)` and logs result
- [ ] HTML calls `prisma.setGlobal(...)` then reads back to confirm write
- [ ] HTML calls `await prisma.getProperty(...)` and logs result
- [ ] HTML calls `prisma.setProperty(...)` then reads back to confirm write
- [ ] Non-existent plugin returns `null` without crash
- [ ] Non-existent property returns `null` without crash
