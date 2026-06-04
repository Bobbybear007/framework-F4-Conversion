# PrismaUI_F4 Example Plugin

A minimal example demonstrating how to build a PrismaUI_F4 plugin for Fallout 4.

## Quick Start

### Prerequisites
- Fallout 4 with F4SE installed
- Visual Studio 2022 with C++ tools
- xmake 3.0+
- MO2 (optional, for mod testing)

### Building

From the parent directory (`E:\F4SE OG\Prisma\PrismaUI_F4 New Gen\`):

```powershell
# Configure (one-time)
xmake f -m release -y

# Build the plugin
xmake build PrismaUI-F4-Example-Plugin
```

The plugin DLL will be built to:
```
build\windows\x64\release\PrismaUI-F4-Example.dll
```

### Deploying to MO2

From the `example-f4se-plugin\` directory:

```powershell
# Run the build and deploy script
.\build.bat

# Or, if already built:
.\deploy.bat
```

Both scripts will prompt for your MO2 mods folder path and copy the plugin DLL to:
```
<MO2 Mods Folder>\ExamplePlugin\F4SE\plugins\PrismaUI-F4-Example.dll
```

### Verifying Deployment

```powershell
.\verify-deploy.bat
```

This checks that the DLL was deployed correctly and isn't empty.

## How It Works

The example plugin demonstrates:

1. **F4SE Plugin Initialization** — Registering message handlers for game events
2. **PrismaUI API Access** — Requesting the IVPrismaUI2 interface at `kGameDataReady`
3. **View Lifecycle** — Creating and managing UI views safely
4. **Event Handling** — Responding to game and UI events

## File Structure

```
example-f4se-plugin/
├── src/
│   ├── main.cpp              — Plugin entry point and initialization
│   ├── keyhandler/           — Input event handling
│   └── PCH.h                 — Precompiled header
├── build.bat                 — Build + deploy script
├── deploy.bat                — Deploy-only script
├── verify-deploy.bat         — Verification script
└── xmake.lua                 — Build configuration (included in parent)
```

## Key Files

- **xmake.lua** — Build target defined in parent xmake.lua (not standalone)
- **PCH.h** — Precompiled header with CommonLibF4 and PrismaUI includes
- **main.cpp** — F4SE plugin initialization and message handler setup

## Troubleshooting

### "Build failed" — xmake compilation error
- Ensure NewCommonLib is built: `cd E:\F4SE OG\Prisma\NewCommonLib && xmake`
- Check vcvars64.bat is in your PATH: `call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`

### "DLL not found at deploy time"
- Run `build.bat` from the plugin directory (not `deploy.bat` alone)
- Verify build succeeded: check for `build\windows\x64\release\PrismaUI-F4-Example.dll` in the parent

### "Empty folders in MO2"
- Check log: `%USERPROFILE%\Documents\My Games\Fallout4\F4SE\PrismaUI-F4-Example-Plugin.log`
- Verify MO2 path is correct when `build.bat` prompts
- Run `verify-deploy.bat` to check file size

## Integration with PrismaUI_F4

This plugin requires PrismaUI_F4 framework to be installed:
- Framework DLL: `PrismaUI_F4.dll` → `F4SE\plugins\`
- Framework files: Ultralight libs + resources → `Data\PrismaUI_F4\`

Deploy PrismaUI_F4 first before loading this plugin.

## Next Steps

After successful build:

1. **Add Views** — Create HTML/CSS/JS files in `view/` for UI
2. **Register Listeners** — Use `RegisterJSListener()` in your message handler
3. **Call JS** — Use `InteropCall()` for frequent calls, `Invoke()` for one-shot reads
4. **Test In-Game** — Load the mod in your profile and check the F4SE log

See `E:\F4SE OG\Prisma\PrismaUI_F4\docs\` for complete API documentation.
