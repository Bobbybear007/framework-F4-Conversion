@echo off
setlocal enabledelayedexpansion

REM Get absolute path to parent directory
for %%I in (%~dp0..) do set PARENT_DIR=%%~fI

set /p DEPLOY_PATH="Where do you want to deploy to? "

if not defined DEPLOY_PATH (
    echo Cancelled
    pause
    exit /b 0
)

REM DLL is built in parent directory
set DLL_PATH=!PARENT_DIR!\build\windows\x64\release\PrismaUI-F4-Example.dll

if not exist "!DLL_PATH!" (
    echo.
    echo ERROR: DLL not found at:
    echo   !DLL_PATH!
    echo.
    echo Build the plugin first:
    echo   .\build.bat
    pause
    exit /b 1
)

echo.
echo Source: !DLL_PATH!
echo Target: !DEPLOY_PATH!\F4SE\plugins\
echo.

if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"

copy /Y "!DLL_PATH!" "!DEPLOY_PATH!\F4SE\plugins\" >nul
if errorlevel 1 (
    echo ERROR: Copy failed
    pause
    exit /b 1
)

echo [OK] PrismaUI-F4-Example.dll deployed

echo.
echo ✓ Done
pause
