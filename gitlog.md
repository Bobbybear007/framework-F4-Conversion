# PrismaUI Framework - Build System & Architecture Update Log

This document details the major structural changes made to the PrismaUI Framework's build system to support dual-targeting for Old-Gen and Next-Gen Fallout 4.

## Major Changes & Features

### 1. Unified `xmake` Build System
- Completely removed the legacy CMake and `BUILD.ps1` PowerShell scripts.
- Transitioned the entire PrismaUI Framework and the Example Plugin to use `xmake.lua`.
- `xmake` now automatically handles linking Ultralight SDK, CommonLibF4, and setting up the correct C++23 flags.

### 2. Dual-Targeting: Old-Gen (OG) and Next-Gen (NG)
- The game's memory offsets changed between 1.10.163 (Old-Gen) and 1.10.984 (Next-Gen). We now have an automated system to compile the plugin for both environments.
- **`PRISMA_TARGET` Environment Variable:** 
  - When set to `og`, the framework compiles against the legacy `CommonLibF4` and explicitly injects the `F4SEPlugin_Query` macro required by F4SE 0.6.23.
  - When set to `ng`, the framework compiles against the modern `lib/commonlibf4` submodule.
- **Distribution Folders:** The build system automatically outputs to distinct distribution folders (e.g., `dist/PrismaUI_F4_1.0.0_OG` vs `dist/PrismaUI_F4_1.0.0_NG`) to prevent overwriting DLLs.

### 3. Automated `build-and-deploy.bat` Scripts
- Consolidated the confusing `build.bat`, `deploy.bat`, and `setup.bat` into a single, unified `build-and-deploy.bat` file for both the framework and the example plugin.
- **Interactive Prompts:** Mod authors just double-click the `.bat` file and are prompted: `Build for Old-Gen (OG) or Next-Gen (NG)? [OG/NG]:`
- The scripts automatically inject the correct environment variable and force an `xmake f -c` clean reconfiguration so no mismatched code is compiled.
- Once compiled, the script seamlessly deploys the resulting DLLs directly into the user's Mod Organizer 2 virtual folders.

### 4. Example Plugin Overhaul
- The `example-f4se-plugin` now fully replicates the dual-targeting architecture.
- Added a dedicated `README.md` to explain the OG vs NG plugin distribution requirements to new mod authors.
- The UI in the example plugin was updated to use an interactive textarea for the event log, and the copy button now unescapes JSON newlines perfectly.

## Recent Commit Log

| Commit Hash | Author | Message |
| ----------- | ------ | ------- |
| `5dc14431` | NomadsReach | refactor: convert example plugin event log to interactive textarea |
| `16bd689b` | NomadsReach | Add dual-targeting for Old-Gen and Next-Gen via PRISMA_TARGET |
| `b0b0afc2` | NomadsReach | refactor: remove verbose comments from PrismaUI_F4 framework |
| `99e6eace` | NomadsReach | remove: property write functionality (unsupported by F4SE) |
| `6aa63139` | NomadsReach | fix: copy button now unescapes JSON newlines before clipboard |
| `31b87b74` | NomadsReach | debug: add copy logging, note property write limitation |
| `c10099e8` | NomadsReach | refactor: remove example paths from deployment prompts |
| `e8bab389` | NomadsReach | feat: add build-and-deploy.bat for example plugin |
| `594ee01d` | NomadsReach | cleanup: remove obsolete build/deploy/verify BAT files |
| `5322faa4` | NomadsReach | feat: add build-and-deploy.bat for PrismaUI_F4 framework |
| `547e1a0c` | NomadsReach | fix: RUN.bat files now call xmake instead of deleted BUILD.ps1 |
