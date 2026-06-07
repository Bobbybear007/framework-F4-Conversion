---
title: 'PrismaMCM'
---
# PrismaMCM - Mod Configuration Menu

PrismaMCM is a companion plugin that gives any mod a settings screen without requiring C++ code. You drop a `config.json` file into your mod's data folder and PrismaMCM renders the controls, reads current values, and writes changes back - directly to Papyrus auto-properties, TESGlobal forms, or INI files.

**Requires:** PrismaUI_F4 + PrismaMCM both installed.

---

## How it works

When a game is loaded, PrismaMCM scans `Data\MCM\Config\` for any subfolder containing a `config.json`. For each one it finds, it reads the current values from your Papyrus scripts or globals, and makes them available in the MCM interface.

Opening the MCM (default **F12**, or the button in the Pause menu):
1. PrismaMCM re-reads all your current values live
2. Displays them in the HTML interface
3. When the player changes a control, PrismaMCM writes the new value back immediately - directly to the Papyrus VM property or TESGlobal, no restart required

---

## File layout

```
Data\
  MCM\
    Config\
      YourModName\
        config.json          ← you write this
    Settings\
      YourModName.ini        ← auto-created for INI-backed settings
  F4SE\
    Plugins\
      PrismaMCM.ini          ← optional: change the hotkey
```

The subfolder name under `Config\` must match the `modName` field in your `config.json`.

---

## Minimal config.json

```json
{
  "modName": "MyMod",
  "displayName": "My Mod",
  "pages": [
    {
      "pageDisplayName": "Settings",
      "content": [
        {
          "type": "slider",
          "id": "DamageScale",
          "text": "Damage Multiplier",
          "valueOptions": {
            "sourceType": "PropertyValueFloat",
            "sourceForm": "MyMod.esp|800",
            "propertyName": "DamageMultiplier",
            "min": 0.5,
            "max": 3.0,
            "step": 0.1
          }
        }
      ]
    }
  ]
}
```

---

## Top-level fields

| Field | Required | Description |
|---|---|---|
| `modName` | Yes | Must match the folder name under `Data\MCM\Config\`. Used to look up the INI file at `Data\MCM\Settings\<modName>.ini`. |
| `displayName` | No | The name shown in the MCM mod list. Defaults to `modName`. Supports `$KEY` translation tokens. |
| `content` | No | Controls shown before the first page (a header area). Same format as page `content`. |
| `pages` | No | Array of page objects. If omitted, all controls go in the header. |

---

## Pages

```json
"pages": [
  {
    "pageDisplayName": "General",
    "content": [ ... ]
  },
  {
    "pageDisplayName": "Audio",
    "content": [ ... ]
  }
]
```

| Field | Description |
|---|---|
| `pageDisplayName` | Tab label shown in the MCM. Supports `$KEY` translation tokens. |
| `content` | Array of control objects for this page. |

---

## Control types

### `section` - visual divider with a heading

```json
{ "type": "section", "text": "Combat Settings" }
```

### `spacer` - blank space

```json
{ "type": "spacer", "numLines": 1 }
```

### `text` - read-only label

```json
{ "type": "text", "text": "Changes take effect immediately." }
```

### `slider` - numeric range picker

```json
{
  "type": "slider",
  "id": "SprintSpeed",
  "text": "Sprint Speed",
  "help": "How fast the player sprints. Default is 1.0.",
  "valueOptions": {
    "sourceType": "PropertyValueFloat",
    "sourceForm": "MyMod.esp|800",
    "propertyName": "SprintSpeed",
    "min": 0.5,
    "max": 3.0,
    "step": 0.05
  }
}
```

### `switcher` - on/off toggle

```json
{
  "type": "switcher",
  "id": "HardcoreMode",
  "text": "Hardcore Mode",
  "help": "Enables permadeath and resource scarcity.",
  "valueOptions": {
    "sourceType": "PropertyValueBool",
    "sourceForm": "MyMod.esp|800",
    "propertyName": "HardcoreMode"
  }
}
```

### `dropdown` - pick from a list

```json
{
  "type": "dropdown",
  "id": "Difficulty",
  "text": "Difficulty",
  "valueOptions": {
    "sourceType": "PropertyValueInt",
    "sourceForm": "MyMod.esp|800",
    "propertyName": "Difficulty",
    "options": ["Easy", "Normal", "Hard", "Survival"]
  }
}
```

The stored value is the zero-based index of the selected option as a string (`"0"`, `"1"`, `"2"`, `"3"`).

### `button` - trigger an action

```json
{
  "type": "button",
  "text": "Reset to Defaults",
  "help": "Resets all settings for this mod.",
  "action": {
    "type": "CallFunction",
    "form": "MyMod.esp|800",
    "function": "ResetDefaults"
  }
}
```

| `action.type` | What it does |
|---|---|
| `CallFunction` | Calls a Papyrus function on the form at `action.form`. `action.function` is the function name. |
| `RunConsoleCommand` | Runs `action.command` as a console command. |

---

## `valueOptions` - source types

Every control that stores a value needs a `valueOptions` block. The `sourceType` field controls where the value is read from and written to.

### `PropertyValueFloat` / `PropertyValueInt` / `PropertyValueBool`

Reads and writes a **Papyrus auto-property** attached to a script on a form.

```json
"valueOptions": {
  "sourceType": "PropertyValueFloat",
  "sourceForm": "MyMod.esp|800",
  "propertyName": "MyPropertyName",
  "min": 0.0,
  "max": 10.0,
  "step": 0.5
}
```

| Field | Description |
|---|---|
| `sourceForm` | `"Plugin.esp|LocalFormID"` - the local (un-merged) form ID in hexadecimal. The form must have a script attached that declares the property. |
| `propertyName` | The exact name of the Papyrus `Auto` property on the script. Case-sensitive. |

**`sourceForm` local ID explained:**

If your quest form is `0x00000800` in xEdit (the `00` file prefix stripped, leaving `000800`), write it as `"MyMod.esp|800"`. Use the last 3 hex digits for normal ESPs - do not include the file index byte.

**Requirement:** The form at `sourceForm` must have a script attached to it in-game with the property already declared. A Quest form is the most reliable choice since it persists across cell changes.

No INI file is written for `PropertyValue*` types. The value lives entirely in the Papyrus VM.

### `GlobalValue`

Reads and writes a `TESGlobal` form's live float value.

```json
"valueOptions": {
  "sourceType": "GlobalValue",
  "sourceForm": "MyMod.esp|801",
  "min": 1,
  "max": 5,
  "step": 1
}
```

The global's value is updated immediately when the player moves the slider. No INI file is written. Papyrus scripts can read the global at any time via `GlobalVariable.GetValue()`.

### `ModSettingFloat` / `ModSettingInt` / `ModSettingBool`

Reads and writes `Data\MCM\Settings\<modName>.ini`. Use this for settings you want to persist as a file without needing a Papyrus script.

```json
"valueOptions": {
  "sourceType": "ModSettingFloat",
  "min": 0.0,
  "max": 1.0,
  "step": 0.01
}
```

The INI key is taken from the control's `id` field. The INI section defaults to `Profile1` unless the `id` contains a colon: `"id": "volume:Audio"` writes to section `Audio`, key `volume`.

---

## Papyrus integration

### Reading a PropertyValue from Papyrus

After PrismaMCM writes a property, the new value is live in the Papyrus VM immediately. Any Papyrus code that accesses the property will see the updated value:

```papyrus
Scriptname MyMod_Quest extends Quest

float Property SprintSpeed = 1.0 Auto
bool  Property HardcoreMode = false Auto
int   Property Difficulty   = 1 Auto

; Papyrus reads the current values whenever it needs them -
; no event required, PrismaMCM has already updated them.
Function ApplySettings()
    Game.SetGameSettingFloat("fMoveSprintMult", SprintSpeed)
EndFunction
```

### Reading a GlobalValue from Papyrus

```papyrus
GlobalVariable Property MyDifficultyGlobal Auto

; Read live value any time
float diff = MyDifficultyGlobal.GetValue()
```

### Detecting when the MCM closes

PrismaMCM is not a Scaleform menu, so `RegisterForMenuOpenCloseEvent("PrismaMCM")` will not fire. The simplest pattern is to read your properties at the natural points in your script flow - on game load, on combat start, on cell change - rather than trying to react to the MCM closing.

For settings that must take effect the moment they change (e.g., adjusting a game setting float), use `GlobalValue` and poll the global in a recurring timer:

```papyrus
Event OnTimer(int aiTimerID)
    if aiTimerID == SETTINGS_TIMER
        Game.SetGameSettingFloat("fMoveSprintMult", SprintSpeedGlobal.GetValue())
        StartTimer(5.0, SETTINGS_TIMER)
    endif
EndEvent
```

---

## Translation support

All `text`, `help`, `displayName`, and `pageDisplayName` fields support `$KEY` tokens that are replaced with strings from a Fallout 4 translation file.

**Translation file locations** (checked in order, first match wins):

```
Data\Interface\Translations\<modName>_<lang>.txt
Data\Interface\Translations\MCM_<modName>_<lang>.txt
Data\MCM\Config\<modName>\Translation\<anything>_<lang>.txt
```

Language is auto-detected from `Fallout4.ini`. Falls back to `en` if the language file is not found.

**File format** (UTF-16 LE with BOM, or plain UTF-8/ASCII):

```
$MY_TITLE	My Mod Settings
$SPRINT_TEXT	Sprint Speed
$SPRINT_HELP	How fast the player sprints.
```

**Using tokens in config.json:**

```json
{
  "modName": "MyMod",
  "$displayName": "$MY_TITLE",
  "pages": [
    {
      "$pageDisplayName": "$PAGE_GENERAL",
      "content": [
        {
          "type": "slider",
          "id": "SprintSpeed",
          "$text": "$SPRINT_TEXT",
          "$help": "$SPRINT_HELP",
          "valueOptions": { ... }
        }
      ]
    }
  ]
}
```

Use `$text` / `$help` / `$displayName` / `$pageDisplayName` (with the `$` prefix on the JSON key) to mark fields for translation lookup. Plain `text` / `help` fields are used as-is.

---

## Hotkey

Default is **F12**. Change it in `Data\F4SE\Plugins\PrismaMCM.ini`:

```ini
[General]
iHotkeyCode=0x7B
```

The value is a BS_BUTTON_CODE (same as `DIK_*` scan codes). Common values:

| Key | Code |
|---|---|
| F12 (default) | `0x7B` |
| F11 | `0x7A` |
| F10 | `0x79` |
| Home | `0x47` |
| Insert | `0x52` |
| Delete | `0x53` |

The MCM button also appears on the vanilla **Pause menu** - no hotkey needed for keyboard-free navigation.

---

## Categories

By default, all mods appear in an UNCATEGORIZED section. To group them:

**`Data\MCM\Config\PrismaMCM_categories.json`:**

```json
{
  "categories": [
    {
      "name": "Combat",
      "mods": ["MyWeaponMod", "MyCombatAI"]
    },
    {
      "name": "Survival",
      "mods": ["MyNeedsSystem", "MyHardcoreMod"]
    }
  ]
}
```

`mods` entries must match the `modName` fields in each mod's `config.json`. Mods not listed fall through to UNCATEGORIZED.

---

## Full config.json example

```json
{
  "modName": "FallenWorldAI",
  "displayName": "Fallen World - AI Settings",

  "pages": [
    {
      "pageDisplayName": "Combat",
      "content": [
        {
          "type": "section",
          "text": "Enemy Behaviour"
        },
        {
          "type": "slider",
          "id": "AggressionScale",
          "text": "Aggression Scale",
          "help": "Multiplies enemy aggression. 1.0 = vanilla.",
          "valueOptions": {
            "sourceType": "PropertyValueFloat",
            "sourceForm": "FallenWorldAI.esp|D63",
            "propertyName": "AggressionScale",
            "min": 0.1,
            "max": 5.0,
            "step": 0.1
          }
        },
        {
          "type": "switcher",
          "id": "FlankingEnabled",
          "text": "Flanking Behaviour",
          "help": "Enemies attempt to circle around the player.",
          "valueOptions": {
            "sourceType": "PropertyValueBool",
            "sourceForm": "FallenWorldAI.esp|D63",
            "propertyName": "FlankingEnabled"
          }
        },
        {
          "type": "dropdown",
          "id": "CombatStyle",
          "text": "Default Combat Style",
          "valueOptions": {
            "sourceType": "PropertyValueInt",
            "sourceForm": "FallenWorldAI.esp|D63",
            "propertyName": "CombatStyle",
            "options": ["Aggressive", "Defensive", "Balanced"]
          }
        }
      ]
    },
    {
      "pageDisplayName": "Performance",
      "content": [
        {
          "type": "section",
          "text": "Actor Limits"
        },
        {
          "type": "slider",
          "id": "MaxCombatActors",
          "text": "Max Combat Actors",
          "help": "Caps the number of actors in active combat at once.",
          "valueOptions": {
            "sourceType": "GlobalValue",
            "sourceForm": "FallenWorldAI.esp|D64",
            "min": 4,
            "max": 32,
            "step": 1
          }
        },
        {
          "type": "spacer"
        },
        {
          "type": "button",
          "text": "Reset All to Defaults",
          "action": {
            "type": "CallFunction",
            "form": "FallenWorldAI.esp|D63",
            "function": "ResetDefaults"
          }
        }
      ]
    }
  ]
}
```

---

## Checklist before testing

- [ ] `modName` in `config.json` matches the folder name under `Data\MCM\Config\`
- [ ] `sourceForm` uses the **local** form ID (strip the file index byte)
- [ ] The form at `sourceForm` has a script attached with the correct property names
- [ ] Properties are declared `Auto` in the Papyrus script
- [ ] PrismaMCM.dll and PrismaUI_F4.dll are both installed
- [ ] F12 opens the MCM (or check `Data\F4SE\Plugins\PrismaMCM.log` for errors)
