@echo off
setlocal enabledelayedexpansion

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars64.bat not found
    pause
    exit /b 1
)

if not exist "build\ultralight-1.4.0\include" (
    echo Extracting Ultralight SDK...
    call setup.bat
    if errorlevel 1 (
        echo ERROR: setup.bat failed
        pause
        exit /b 1
    )
)

echo.
echo Building PrismaUI_F4...
xmake f -m release -y
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

xmake build
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ✓ Build successful
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
