@echo off
setlocal enabledelayedexpansion

REM Get absolute path to parent directory
for %%I in (%~dp0..) do set PARENT_DIR=%%~fI

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars64.bat not found
    pause
    exit /b 1
)

if not exist "!PARENT_DIR!\build\ultralight-1.4.0\include" (
    echo Extracting Ultralight SDK...
    call "!PARENT_DIR!\setup.bat"
    if errorlevel 1 (
        echo ERROR: setup.bat failed
        pause
        exit /b 1
    )
)

echo.
echo Building PrismaUI-F4-Example Plugin...
echo Working directory: !PARENT_DIR!
echo.

pushd "!PARENT_DIR!"
xmake f -m release -y
if errorlevel 1 (
    echo ERROR: Configuration failed
    popd
    pause
    exit /b 1
)

xmake build PrismaUI-F4-Example-Plugin
if errorlevel 1 (
    echo ERROR: Build failed
    popd
    pause
    exit /b 1
)
popd

REM Check if DLL was built
set DLL_PATH=!PARENT_DIR!\build\windows\x64\release\PrismaUI-F4-Example.dll
if not exist "!DLL_PATH!" (
    echo.
    echo ERROR: DLL not found at:
    echo   !DLL_PATH!
    pause
    exit /b 1
)

echo.
echo ✓ Build successful: !DLL_PATH!
echo.
set /p DEPLOY_PATH="Where do you want to deploy to? "

if not defined DEPLOY_PATH (
    echo Cancelled
    pause
    exit /b 0
)

echo.
echo Deploying to: !DEPLOY_PATH!
echo.

if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"

copy /Y "!DLL_PATH!" "!DEPLOY_PATH!\F4SE\plugins\" >nul
if errorlevel 1 (
    echo ERROR: Deploy failed
    pause
    exit /b 1
)

echo [OK] PrismaUI-F4-Example.dll deployed

echo.
echo ✓ Done
pause
