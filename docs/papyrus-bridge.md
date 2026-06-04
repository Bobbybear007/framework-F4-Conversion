---
title: 'Papyrus Bridge'
---
# Papyrus Bridge

PrismaUI_F4 automatically injects `window.prisma` into every view before `OnDomReadyCallback` fires. Papyrus modders can read and write live game data directly from JavaScript — no C++ plugin required.

---

## Overview

Every HTML view created by any plugin (V1 or later) gets `window.prisma` for free. You don't request it, configure it, or install anything extra. If PrismaUI_F4 is installed, `window.prisma` is there.

```js
// window.prisma is available as soon as DOM ready fires.
// No RequestPluginAPI, no view handle, no C++.

const damage = await prisma.getProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale");
prisma.setProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale", 2.5);

const diff = await prisma.getGlobal("MyMod.esp", "801");
prisma.setGlobal("MyMod.esp", "801", 3.0);
```

---

## Methods

### `prisma.getGlobal(esp, formId)`

Reads a `TESGlobal` form's float value.

| Parameter | Type | Description |
|---|---|---|
| `esp` | `string` | Plugin filename including extension — `"MyMod.esp"` or `"MyMod.esl"` |
| `formId` | `string` | Local hex form ID without the file-index byte: `"800"` = `0x00000800` in the plugin |

**Returns:** `Promise<number | null>`

---

### `prisma.setGlobal(esp, formId, value)`

Writes a `TESGlobal` form's float value. Fire-and-forget — returns `undefined`.

| Parameter | Type | Description |
|---|---|---|
| `esp` | `string` | Plugin filename |
| `formId` | `string` | Local hex form ID |
| `value` | `number` | New float value |

---

### `prisma.getProperty(esp, formId, scriptName, propName)`

Reads a Papyrus `Auto` property on a script attached to a form.

| Parameter | Type | Description |
|---|---|---|
| `esp` | `string` | Plugin filename |
| `formId` | `string` | Local hex form ID |
| `scriptName` | `string` | Exact name of the Papyrus script (case-insensitive) |
| `propName` | `string` | Exact name of the `Auto` property (case-insensitive) |

**Returns:** `Promise<number | boolean | null>`

---

### `prisma.setProperty(esp, formId, scriptName, propName, value)`

Writes a Papyrus `Auto` property. Fire-and-forget — returns `undefined`. The value is coerced to the property's declared type (`float`, `int`, or `bool`) automatically.

| Parameter | Type | Description |
|---|---|---|
| `esp` | `string` | Plugin filename |
| `formId` | `string` | Local hex form ID |
| `scriptName` | `string` | Papyrus script name |
| `propName` | `string` | Property name |
| `value` | `number \| boolean \| string` | New value — coerced to declared type |

---

## Form ID format

`formId` is the **local** form ID — strip the file-index byte.

| xEdit shows | Write as |
|---|---|
| `00000800` | `"800"` |
| `00000D63` | `"D63"` |

ESL forms use the same convention. PrismaUI_F4 resolves the ESL prefix automatically via `TESDataHandler::LookupForm`.

---

## Null return

Both read methods return `null` (never throw) when:

- The plugin is not in the active load order
- The form ID does not exist in that plugin
- The form exists but is the wrong type (not `TESGlobal`, or script not attached)
- The property name is not found or not declared `Auto`
- The Papyrus VM is not yet ready — always call after `kPostLoadGame`

```js
const val = await prisma.getGlobal("MyMod.esp", "800");
if (val === null) {
  // plugin absent, form not found, or VM not ready
  return;
}
```

---

## Supported property types

| Type | Supported |
|---|---|
| `float` | ✓ |
| `int` | ✓ |
| `bool` | ✓ |
| `string` | ✗ not in v1 |
| Arrays | ✗ not in v1 |

---

## Full example

**Papyrus script (Creation Kit):**

```papyrus
Scriptname MyMod_QuestScript extends Quest

float Property DamageScale  = 1.0   Auto
bool  Property HardcoreMode = false Auto
int   Property Difficulty   = 1     Auto
```

Attach this script to a Quest form. Quest forms are the most reliable host — they persist across cell changes and the Papyrus VM always has them loaded.

**HTML view:**

```html
<script>
  async function loadSettings() {
    const dmg  = await prisma.getProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale");
    const hard = await prisma.getProperty("MyMod.esp", "800", "MyMod_QuestScript", "HardcoreMode");

    if (dmg  !== null) document.getElementById("damage").value   = dmg;
    if (hard !== null) document.getElementById("hardcore").checked = hard;
  }

  function saveDamage(val) {
    prisma.setProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale", parseFloat(val));
  }

  function saveHardcore(checked) {
    prisma.setProperty("MyMod.esp", "800", "MyMod_QuestScript", "HardcoreMode", checked);
  }
</script>
```

---

## Requirements

- **Quest form host** — use a persistent Quest form, not a cell-local reference
- **After `kPostLoadGame`** — the Papyrus VM is not ready before the game loads; settings menus opened in-game are always safe
- **PrismaUI_F4 installed** — `window.prisma` is injected by the framework; without it the object is `undefined`

---

## Threading model

JS calls to `window.prisma` fire on the Ultralight render thread. PrismaUI_F4 marshals all `RE::` game-data access to the game thread via `F4SE::GetTaskInterface()->AddTask` internally. Responses are delivered back to JS as Promise resolutions via `_prismaResponse`. This is transparent — from JS the API is just async functions.
