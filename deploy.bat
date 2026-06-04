@echo off
setlocal enabledelayedexpansion

set /p DEPLOY_PATH="Where do you want to deploy to? "

if not defined DEPLOY_PATH (
    echo Cancelled
    pause
    exit /b 0
)

if not exist "build\windows\x64\release\PrismaUI_F4.dll" (
    echo ERROR: Build first
    pause
    exit /b 1
)

if not exist "build\ultralight-1.4.0\bin\AppCore.dll" (
    echo ERROR: Run setup.bat first
    pause
    exit /b 1
)

echo.
echo Deploying to: !DEPLOY_PATH!
echo.

if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\libs" mkdir "!DEPLOY_PATH!\PrismaUI_F4\libs"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\resources" mkdir "!DEPLOY_PATH!\PrismaUI_F4\resources"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\misc" mkdir "!DEPLOY_PATH!\PrismaUI_F4\misc"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\icons" mkdir "!DEPLOY_PATH!\PrismaUI_F4\icons"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\assets" mkdir "!DEPLOY_PATH!\PrismaUI_F4\assets"

copy /Y "build\windows\x64\release\PrismaUI_F4.dll" "!DEPLOY_PATH!\F4SE\plugins\" >nul
echo [OK] PrismaUI_F4.dll

for %%f in (AppCore.dll Ultralight.dll UltralightCore.dll WebCore.dll) do (
    copy /Y "build\ultralight-1.4.0\bin\%%f" "!DEPLOY_PATH!\PrismaUI_F4\libs\" >nul
    echo [OK] %%f
)

for %%f in (cacert.pem icudt67l.dat) do (
    copy /Y "build\ultralight-1.4.0\resources\%%f" "!DEPLOY_PATH!\PrismaUI_F4\resources\" >nul
    echo [OK] %%f
)

if exist "assets\misc\cursor.png" (
    copy /Y "assets\misc\cursor.png" "!DEPLOY_PATH!\PrismaUI_F4\misc\" >nul
    echo [OK] cursor.png
)

echo.
echo ✓ Done
pause
