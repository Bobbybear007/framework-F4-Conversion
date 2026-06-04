@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   PrismaUI_F4 Framework
echo   Complete Build, Setup & Deploy
echo ========================================
echo.

echo Checking GitHub for framework updates...
for /f "tokens=*" %%A in ('powershell -Command "try { $resp = Invoke-RestMethod -Uri 'https://api.github.com/repos/NomadsReach/framework-F4-Conversion/releases/latest' -ErrorAction Stop; Write-Output $resp.tag_name } catch { Write-Output 'error' }"') do set GITHUB_VERSION=%%A

if not "!GITHUB_VERSION!"=="error" (
    if not "!GITHUB_VERSION!"=="" (
        echo Found GitHub release: !GITHUB_VERSION!
        echo.
        echo ==========================================
        echo   WARNING: NEWER VERSION AVAILABLE
        echo ==========================================
        echo GitHub has version: !GITHUB_VERSION!
        echo You should pull the latest framework from:
        echo https://github.com/NomadsReach/framework-F4-Conversion
        echo.
        set /p UPDATE="Pull latest from GitHub before building? (Y/N): "
        if /i "!UPDATE!"=="Y" (
            echo Please clone/pull the latest from GitHub, then re-run this script.
            pause
            exit /b 0
        )
    )
)

echo.
if not exist "build\external_builds\ultralight\bin" (
    echo ========================================
    echo STEP 1: Setting up Ultralight SDK...
    echo ========================================

    if not exist "..\UltraBright 1.4.0\bin" (
        echo ERROR: UltraBright SDK not found at ..\UltraBright 1.4.0\
        echo Please ensure the SDK is available in the Prisma directory.
        pause
        exit /b 1
    )

    echo Creating build directories...
    if not exist "build\external_builds\ultralight\bin" mkdir "build\external_builds\ultralight\bin"
    if not exist "build\external_builds\ultralight\resources" mkdir "build\external_builds\ultralight\resources"

    echo Copying Ultralight SDK files...
    xcopy /Y /I "..\UltraBright 1.4.0\bin\*.dll" "build\external_builds\ultralight\bin\"
    xcopy /Y /I "..\UltraBright 1.4.0\resources\*" "build\external_builds\ultralight\resources\"

    if errorlevel 1 (
        echo ERROR: Failed to copy SDK files
        pause
        exit /b 1
    )
    echo SDK setup complete.
    echo.
)

if exist "build\windows\x64\release\PrismaUI_F4.dll" (
    echo Checking PrismaUI_F4 source code for updates...

    for %%F in (build\windows\x64\release\PrismaUI_F4.dll) do set DLL_TIME=%%~tF

    set SOURCE_NEWER=0
    for /r "..\PrismaUI" %%F in (*.cpp *.h) do (
        if "%%~tF" gtr "!DLL_TIME!" (
            set SOURCE_NEWER=1
        )
    )

    if !SOURCE_NEWER! equ 1 (
        echo.
        echo ========================================
        echo   WARNING: PrismaUI_F4 SOURCE UPDATED
        echo ========================================
        echo PrismaUI source code is NEWER than the built DLL.
        echo Proceeding with rebuild...
        echo.
    )
)

echo ========================================
echo STEP 2: Building PrismaUI_F4 framework...
echo ========================================
echo.
set /p TARGET_VER="Build for Old-Gen (OG) or Next-Gen (NG)? [OG/NG]: "
if /i "!TARGET_VER!"=="OG" (
    set PRISMA_TARGET=og
    echo Patching Old-Gen CommonLibF4 template...
    copy /Y "scripts\commonlibf4-plugin.cpp.in" "..\CommonLibF4\res\commonlibf4-plugin.cpp.in" >nul
) else (
    set PRISMA_TARGET=ng
    echo Patching Next-Gen CommonLibF4 submodule template...
    copy /Y "scripts\commonlibf4-plugin.cpp.in" "lib\commonlibf4\res\commonlibf4-plugin.cpp.in" >nul
)
echo Building for !TARGET_VER! ...
xmake f -c
xmake
if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo Build successful!
echo.

echo ========================================
echo STEP 3: Deployment
echo ========================================
echo.
set /p DEPLOY_PATH="Enter deployment path: "

if "!DEPLOY_PATH!"=="" (
    echo ERROR: No path provided
    pause
    exit /b 1
)

if not exist "build\external_builds\ultralight\bin\AppCore.dll" (
    echo ERROR: Ultralight SDK files missing from build output
    pause
    exit /b 1
)

echo.
echo Creating directories...
if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\libs" mkdir "!DEPLOY_PATH!\PrismaUI_F4\libs"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\resources" mkdir "!DEPLOY_PATH!\PrismaUI_F4\resources"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\misc" mkdir "!DEPLOY_PATH!\PrismaUI_F4\misc"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\icons" mkdir "!DEPLOY_PATH!\PrismaUI_F4\icons"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\views" mkdir "!DEPLOY_PATH!\PrismaUI_F4\views"

echo Deploying DLL...
copy /Y "build\windows\x64\release\PrismaUI_F4.dll" "!DEPLOY_PATH!\F4SE\plugins\PrismaUI_F4.dll"
if errorlevel 1 (
    echo ERROR: Failed to copy DLL
    pause
    exit /b 1
)

echo Deploying Ultralight libraries...
copy /Y "build\external_builds\ultralight\bin\AppCore.dll" "!DEPLOY_PATH!\PrismaUI_F4\libs\AppCore.dll"
copy /Y "build\external_builds\ultralight\bin\Ultralight.dll" "!DEPLOY_PATH!\PrismaUI_F4\libs\Ultralight.dll"
copy /Y "build\external_builds\ultralight\bin\UltralightCore.dll" "!DEPLOY_PATH!\PrismaUI_F4\libs\UltralightCore.dll"
copy /Y "build\external_builds\ultralight\bin\WebCore.dll" "!DEPLOY_PATH!\PrismaUI_F4\libs\WebCore.dll"

echo Deploying resources...
copy /Y "build\external_builds\ultralight\resources\cacert.pem" "!DEPLOY_PATH!\PrismaUI_F4\resources\cacert.pem"
copy /Y "build\external_builds\ultralight\resources\icudt67l.dat" "!DEPLOY_PATH!\PrismaUI_F4\resources\icudt67l.dat"

echo Deploying cursor asset...
if exist "assets\misc\cursor.png" (
    copy /Y "assets\misc\cursor.png" "!DEPLOY_PATH!\PrismaUI_F4\misc\cursor.png"
)

echo Deploying icons...
if exist "assets\icons" (
    xcopy /Y /E "assets\icons\*" "!DEPLOY_PATH!\PrismaUI_F4\icons\"
)

echo Deploying views...
if exist "assets\views" (
    xcopy /Y /E "assets\views\*" "!DEPLOY_PATH!\PrismaUI_F4\views\"
)

echo.
echo ========================================
echo   SUCCESS: Deployment Complete!
echo ========================================
echo Framework deployed to: !DEPLOY_PATH!
echo.
echo NOTE: PrismaUI_F4 framework has been rebuilt.
echo All plugins that depend on it MUST be rebuilt.
echo Run: example-f4se-plugin\build-and-deploy.bat
echo.
pause
