---
title: 'Translations'
---
# Translations

PrismaUI F4 supports the standard Fallout 4 translation file format. Call `RegisterTranslations` once after `CreateView` and the framework handles everything else - detecting the game language, loading the right file, and injecting `window.L10N` / `window.t()` into your page's JS context on every load.

---

## File format

Translation files use the same format Bethesda uses for vanilla Fallout 4 UI strings:

- **Encoding:** UTF-16 LE with BOM (`FF FE`)
- **One entry per line:** `$KEY<tab>Translated string`
- **Keys start with `$`**
- Lines that don't start with `$` are ignored (use them for comments)

```
$CLOSE	Close
$MY_MENU_TITLE	My Menu
$ITEM_COUNT	Items: {0}
```

UTF-8 files (no BOM) are also accepted for convenience during development, but the game's own tools produce UTF-16 LE, so use that for release.

---

## File location

```
Data\Interface\Translations\<PluginName>_<lang>.txt
```

| Part | Example |
|------|---------|
| `PluginName` | Matches what you pass to `RegisterTranslations` - your plugin's base name without `.esp`/`.esm`/`.dll` |
| `lang` | Lowercase language code detected from the player's INI |

Full example for English:
```
Data\Interface\Translations\MyPlugin_F4_en.txt
```

The framework looks up `<PluginName>_<lang>.txt` first. If that file is missing and the language is not English, it falls back to `<PluginName>_en.txt`.

---

## Language codes

| Code | Language |
|------|----------|
| `en` | English (default fallback) |
| `de` | German |
| `fr` | French |
| `it` | Italian |
| `es` | Spanish |
| `esmx` | Spanish (Mexico) |
| `cn` | Chinese (Simplified) |
| `ja` | Japanese |
| `pl` | Polish |
| `ru` | Russian |
| `ptbr` | Portuguese (Brazil) |

---

## Language detection

The framework reads `sLanguage` from the player's INI files in this priority order:

1. `%USERPROFILE%\Documents\My Games\Fallout4\Fallout4Custom.ini`
2. `%USERPROFILE%\Documents\My Games\Fallout4\Fallout4Prefs.ini`
3. `<GameDir>\Fallout4.ini`

The first file that contains `sLanguage` under `[General]` wins. This matches how the game itself picks a language.

---

## C++ setup

```cpp
// kPostLoadGame / kNewGame:
g_view = g_api->CreateView("mymenu.html", OnDomReady);
g_api->RegisterTranslations(g_view, "MyPlugin_F4");
g_api->Hide(g_view);
```

That's all. The framework detects the language, loads the file, and re-injects translations automatically every time the page reloads.

---

## JavaScript usage

After `RegisterTranslations`, every page load has access to two globals:

```javascript
// Look up a key - returns the translated string, or the key if not found
window.t('$CLOSE')           // → "Close"
window.t('$MY_MENU_TITLE')   // → "My Menu"
window.t('$MISSING_KEY')     // → "$MISSING_KEY" (key returned as-is)

// The raw lookup table if you need direct access
window.L10N['$CLOSE']        // → "Close"
```

`window.t` is available from the moment `window` exists - before `DOMContentLoaded`, before any `<script>` tags execute. You can use it directly in inline scripts:

```html
<script>
  document.getElementById('close-btn').textContent = window.t('$CLOSE');
</script>
```

Or in a framework component:

```javascript
// React / Vue / plain JS - all work the same
const label = window.t('$CONFIRM');
```

---

## Example mod folder layout

```
mods/MyPlugin_F4/
├── F4SE/Plugins/
│   └── MyPlugin_F4.dll
├── PrismaUI_F4/
│   └── views/
│       └── mymenu.html
└── Interface/
    └── Translations/
        ├── MyPlugin_F4_en.txt
        ├── MyPlugin_F4_de.txt
        └── MyPlugin_F4_fr.txt
```

---

## Notes

- If no translation file is found for the detected language and no English fallback exists, `window.L10N` is not injected. `window.t` will not be defined. Guard against this if translations are optional: `const t = window.t ?? (k => k);`
- Keys are case-sensitive. `$Close` and `$CLOSE` are different keys.
- Values can contain any characters including HTML - escape them yourself before inserting into `innerHTML`.
