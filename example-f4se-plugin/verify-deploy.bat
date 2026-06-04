@echo off
setlocal enabledelayedexpansion

REM Get absolute path to parent directory
for %%I in (%~dp0..) do set PARENT_DIR=%%~fI

set /p DEPLOY_PATH="Where do you want to check? "

if not defined DEPLOY_PATH (
    echo Cancelled
    pause
    exit /b 0
)

set FAIL_COUNT=0

echo.
echo Checking deployment: !DEPLOY_PATH!
echo.

REM Check plugin DLL
if exist "!DEPLOY_PATH!\F4SE\plugins\PrismaUI-F4-Example.dll" (
    for %%I in ("!DEPLOY_PATH!\F4SE\plugins\PrismaUI-F4-Example.dll") do set /a DLL_SIZE=%%~zI
    if !DLL_SIZE! gtr 100000 (
        echo [OK] PrismaUI-F4-Example.dll (!DLL_SIZE! bytes^)
    ) else (
        echo [ERROR] PrismaUI-F4-Example.dll is suspiciously small (!DLL_SIZE! bytes^) - may be corrupt
        set /a FAIL_COUNT+=1
    )
) else (
    echo [MISSING] PrismaUI-F4-Example.dll
    set /a FAIL_COUNT+=1
)

REM Check build source
echo.
echo Build source: !PARENT_DIR!\build\windows\x64\release\PrismaUI-F4-Example.dll
if exist "!PARENT_DIR!\build\windows\x64\release\PrismaUI-F4-Example.dll" (
    for %%I in ("!PARENT_DIR!\build\windows\x64\release\PrismaUI-F4-Example.dll") do set /a BUILD_SIZE=%%~zI
    echo [OK] Built DLL (!BUILD_SIZE! bytes^)
) else (
    echo [MISSING] Built DLL - run .\build.bat first
    set /a FAIL_COUNT+=1
)

echo.
if !FAIL_COUNT! equ 0 (
    echo ✓ Deployment verified
) else (
    echo ✗ !FAIL_COUNT! issues found
)

echo.
pause
