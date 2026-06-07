# PrismaUI_F4 New Gen - Build Instructions

## Quick Build (No Web Interface)

Use `build.bat` instead of the web tool for direct, simple builds.

### Usage

```batch
build.bat "C:\path\to\deploy" [clean]
```

### Examples

**Build and deploy to test folder:**
```batch
build.bat "E:\F4SE OG\Prisma\TestCompile\Framework"
```

**Clean build + deploy to MO2:**
```batch
build.bat "E:\Modlists\Fallen World Alpha 2\mods" clean
```

**Clean build + deploy to test:**
```batch
build.bat "E:\F4SE OG\Prisma\TestCompile\Framework" clean
```

### What It Does

1. Sets deployment environment variable (`XSE_FO4_MODS_PATH`)
2. Optionally cleans old build artifacts (if `clean` argument passed)
3. Configures xmake (`xmake f`)
4. Builds PrismaUI_F4 target (`xmake build -r PrismaUI_F4`)
5. Verifies DLL exists and shows file size

### Output

DLL is deployed to: `<deployment-path>\PrismaUI_F4\F4SE\Plugins\PrismaUI_F4.dll`

### Requirements

- xmake installed and in PATH
- Visual Studio 2022 with C++ build tools
- CommonLibF4 submodule initialized (`git submodule update --recursive`)
- Ultralight SDK 1.4.0 at `build/ultralight-1.4.0/`
