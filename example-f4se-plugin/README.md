# PrismaUI Example Plugin

This is a complete, ready-to-compile example of a Fallout 4 F4SE plugin that uses the PrismaUI framework to render an HTML/JS/CSS interface in-game.

## Dual-Targeting: Old-Gen vs Next-Gen

Fallout 4 was heavily updated in the "Next-Gen" patch (1.10.984). Because your C++ plugin communicates directly with F4SE and the game engine, **you must compile two different versions of your DLL** if you want to support all users. 
- A plugin compiled for Next-Gen will crash if loaded into Old-Gen (1.10.163).
- A plugin compiled for Old-Gen will be ignored or crash if loaded into Next-Gen.

However, your UI files (`.html`, `.css`, `.js`) are universal and only need to be written once!

### How to Compile

We have provided a fully automated build script that handles switching the game version offsets for you using `xmake`.

1. Double-click the `build-and-deploy.bat` file in this directory.
2. The script will ask you: `Build for Old-Gen (OG) or Next-Gen (NG)? [OG/NG]:`
3. Type `OG` and hit Enter. The script will compile your Old-Gen DLL and deploy it.
4. Run the script a second time, type `NG`, and hit Enter. The script will compile your Next-Gen DLL.

### Publishing to NexusMods

When uploading your mod, you should provide both DLLs to your users. The easiest way to do this is to use a [FOMOD Installer](https://wiki.nexusmods.com/index.php/How_to_create_Mod_Installers). This allows the user's Mod Manager (like MO2 or Vortex) to automatically detect their game version and install the correct DLL, while installing your universal UI files regardless of their version.

## File Structure

```
example-f4se-plugin/
├── src/
│   ├── main.cpp              — Plugin entry point and initialization
│   ├── keyhandler/           — Input event handling
│   └── PCH.h                 — Precompiled header
├── view/                     — Your HTML/JS/CSS frontend
├── build-and-deploy.bat      — Automated dual-targeting build script
└── xmake.lua                 — Build configuration
```

## Integration with PrismaUI_F4

This plugin requires the PrismaUI_F4 framework to be installed:
- Framework DLL: `PrismaUI_F4.dll`
- Framework dependencies: Ultralight libs + resources

Make sure you have deployed the framework before loading your plugin in-game.
