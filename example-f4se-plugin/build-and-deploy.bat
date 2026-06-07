@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   PrismaUI Example Plugin
echo   Complete Build and Deploy
echo ========================================
echo.

echo Checking GitHub for framework updates...
set GITHUB_VERSION=error
where pwsh >nul 2>&1 && (
    for /f "tokens=*" %%A in ('pwsh -Command "try { $resp = Invoke-RestMethod -Uri 'https://api.github.com/repos/NomadsReach/framework-F4-Conversion/releases/latest' -ErrorAction Stop; Write-Output $resp.tag_name } catch { Write-Output 'error' }"') do set GITHUB_VERSION=%%A
) || (
    where powershell >nul 2>&1 && (
        for /f "tokens=*" %%A in ('powershell -Command "try { $resp = Invoke-RestMethod -Uri 'https://api.github.com/repos/NomadsReach/framework-F4-Conversion/releases/latest' -ErrorAction Stop; Write-Output $resp.tag_name } catch { Write-Output 'error' }"') do set GITHUB_VERSION=%%A
    )
)

if not "!GITHUB_VERSION!"=="error" (
    if not "!GITHUB_VERSION!"=="" (
        echo Found GitHub release: !GITHUB_VERSION!
        echo.
        echo ==========================================
        echo   WARNING: NEWER FRAMEWORK AVAILABLE
        echo ==========================================
        echo GitHub has version: !GITHUB_VERSION!
        echo You should pull the latest framework from:
        echo https://github.com/NomadsReach/framework-F4-Conversion
        echo before building this plugin against it.
        echo.
        set /p UPDATE="Pull latest from GitHub before building? (Y/N): "
        if /i "!UPDATE!"=="Y" (
            echo Please update the framework, then re-run this script.
            pause
            exit /b 0
        )
    )
)

echo.
if exist "..\build\windows\x64\release\PrismaUI_F4.dll" (
    if exist "build\windows\x64\release\PrismaUI-F4-Example.dll" (
        echo Checking PrismaUI_F4 framework for updates...

        for %%F in (build\windows\x64\release\PrismaUI-F4-Example.dll) do set PLUGIN_TIME=%%~tF
        for %%F in (..\build\windows\x64\release\PrismaUI_F4.dll) do set FRAMEWORK_TIME=%%~tF

        if "!FRAMEWORK_TIME!" gtr "!PLUGIN_TIME!" (
            echo.
            echo ========================================
            echo   WARNING: PrismaUI_F4 FRAMEWORK UPDATED
            echo ========================================
            echo The PrismaUI_F4 framework DLL is NEWER than this plugin.
            echo You MUST rebuild this plugin against the new framework.
            echo.
            set /p CONTINUE="Continue with build? (Y/N): "
            if /i not "!CONTINUE!"=="Y" (
                echo Build cancelled.
                pause
                exit /b 0
            )
        )
    )
)

echo ========================================
echo STEP 1: Building PrismaUI-F4-Example...
echo ========================================
echo.
echo Building (single DLL - supports OG + NG + GOG)...
cd /d "%~dp0.."
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

if not exist "build\windows\x64\release\PrismaUI-F4-Example.dll" (
    echo.
    echo ========================================
    echo   ERROR: Build Output Not Found
    echo ========================================
    echo The plugin DLL was not created.
    echo Check the xmake output above for errors.
    echo.
    pause
    exit /b 1
)

echo ========================================
echo STEP 2: Deployment
echo ========================================
echo.
set /p DEPLOY_PATH="Enter deployment path: "

if "!DEPLOY_PATH!"=="" (
    echo ERROR: No path provided
    pause
    exit /b 1
)

if not exist "!DEPLOY_PATH!" (
    echo ERROR: Deployment path does not exist: !DEPLOY_PATH!
    pause
    exit /b 1
)

echo.
echo Creating directories...
if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example" mkdir "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example"

echo Deploying DLL...
copy /Y "build\windows\x64\release\PrismaUI-F4-Example.dll" "!DEPLOY_PATH!\F4SE\plugins\PrismaUI-F4-Example.dll"
if errorlevel 1 (
    echo ERROR: Failed to copy DLL
    pause
    exit /b 1
)

echo Deploying view files...
if not exist "example-f4se-plugin\view\index.html" (
    echo ERROR: View files not found at example-f4se-plugin\view\
    pause
    exit /b 1
)
copy /Y "example-f4se-plugin\view\index.html" "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example\index.html"

echo.
echo ========================================
echo   SUCCESS: Deployment Complete!
echo ========================================
echo Plugin deployed to: !DEPLOY_PATH!
echo.
pause
