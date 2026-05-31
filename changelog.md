# Version 1.4

docs(prismaui): update API reference, examples, getting-started, view-lifecycle for V4
- IVPrismaUI4 / BindUIEvent added throughout
- RegisterTranslations (V3) documented in api-reference and examples
- All examples updated from IVPrismaUI3 to IVPrismaUI4
- "Updating Your Plugin" migration section in api-reference
- DOM Ready callback section shows BindUIEvent vs RegisterJSListener guidance****

# Version 1.3

## Runtime Compatibility

* Added `g_pluginVersionData` export for F4SE 0.7.1+ (Next-Gen F4SE). Plugins now register through `PluginVersionData` instead of, or alongside, the legacy `F4SEPlugin_Query` path. This allows F4SE to validate the plugin before calling load functions.
* Added `UsesAddressLibrary(true)` support, enabling automatic `.bin` lookup based on the game's runtime version string (example: `version-1-11-221-0.bin`).
* Added `IsLayoutDependent(true)` declaration for plugins using `RE::` types from CommonLibF4.
* Empty `CompatibleVersions` array now signals compatibility with all future runtime versions, removing the need to update supported version lists after game patches.
* Legacy `F4SEPlugin_Query` and `F4SEPlugin_Load` exports are still included for backwards compatibility. Pre-0.7.1 F4SE versions ignore `g_pluginVersionData` and continue using the classic loading path.
* Added minimum supported runtime validation. Game versions below `1.10.162` are now rejected in `F4SEPlugin_Query` due to incompatible pre-Next-Gen hooks.
* `AddressBins/` now ships `.bin` files covering runtimes back to `1.10.130`, supporting all compatible game versions out of the box.

## Core Framework

* Added Ultralight-powered HTML/CSS/JS rendering layer integrated into DXGI `Present` and `ResizeBuffers` using MinHook vtable patching.
* Added versioned plugin API exports:

  * `IVPrismaUI1`
  * `IVPrismaUI2`
  * `IVPrismaUI3`
* APIs are exposed through `RequestPluginAPI` using `GetProcAddress`, removing link-time dependencies for consumer plugins.
* Added full view lifecycle management:

  * `CreateView`
  * `Show`
  * `Hide`
  * `Focus`
  * `Unfocus`
  * `IsValid`
  * `Destroy`
  * `SetOrder`
* Added `InteropCall` for high-frequency C++ to JavaScript communication.
* Added `Invoke` for one-shot JavaScript execution with callback support.
* Added `RegisterJSListener` to expose named window-level JavaScript functions directly to C++.
* Added `RegisterConsoleCallback` (V2) for receiving JavaScript console messages (`log`, `warn`, `error`, `debug`, `info`) in C++.
* Added `RegisterTranslations` (V3), which loads translation files from:

  * `Data\Interface\Translations\<plugin>_<lang>.txt`
* Translation system now injects `window.L10N` and `window.t()` into every view on page load.
* Added inspector/debugging view support:

  * `CreateInspectorView`
  * `SetInspectorVisibility`
  * `SetInspectorBounds`

---

# Version 1.2

Large update to PrismaDesigner.

* Refactored canvas item handling so inserted elements are now managed through a reference-based system internally.
* Continued groundwork for future editor and layout improvements.
* General stability and architecture cleanup across the designer pipeline.

More work is still planned for this system, but this update was pushed out early so people could start testing the new foundation.

---

# Version 1.1

## Bug Fixes

* Fixed Windows 10 crashing issues by upgrading Ultralight from `1.4.1-dev` (pre-release) to `1.4.0` stable.
* The `1.4.1-dev` build contained a heap corruption issue in `WebCore.dll` that caused random crashes across unrelated DLLs including:

  * `usvfs`
  * `ConsoleAutocomplete`
  * `Qt6Network`
* Crashes typically occurred within minutes of loading. Windows 11 systems were unaffected.
* Fixed hook conflict caused by enabling all MinHook hooks globally (`MH_ALL_HOOKS`) during startup.
* PrismaUI now enables only its own Direct3D hooks:

  * `Present`
  * `ResizeBuffers`

## Improvements

* Added required `RefreshDisplay()` call to the render loop per Ultralight `1.4.0` specifications.
* CSS animations, scrolling, and `requestAnimationFrame()` now function correctly.

## PrismaDesigner

* Added the Overseer Fallout-style font.
* Overseer is now available in the font dropdown for all text elements.
* Exported HTML automatically embeds the font as base64, making exported views fully self-contained.
* Added a new Heading element to the palette.
* Heading elements default to:

  * Overseer font
  * 48px size
  * Amber coloring
* Titles can now be dropped directly into layouts with one click.

---

If you were experiencing crashes on Windows 10, update `PrismaUI_F4` and replace the DLLs inside:

`PrismaUI_F4\libs\`

with the DLLs included in this release.

Changelog:
https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer/blob/main/CHANGELOG.MD
