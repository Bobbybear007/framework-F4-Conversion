@echo off
setlocal enabledelayedexpansion

set /p DEPLOY_PATH="Where do you want to check? "

if not defined DEPLOY_PATH (
    echo Cancelled
    pause
    exit /b 0
)

set FAIL_COUNT=0

echo.
echo Checking: !DEPLOY_PATH!
echo.

echo Folder structure:
if exist "!DEPLOY_PATH!\F4SE\plugins" (
    echo [OK] F4SE\plugins
) else (
    echo [MISSING] F4SE\plugins
    set /a FAIL_COUNT+=1
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\libs" (
    echo [OK] PrismaUI_F4\libs
) else (
    echo [MISSING] PrismaUI_F4\libs
    set /a FAIL_COUNT+=1
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\resources" (
    echo [OK] PrismaUI_F4\resources
) else (
    echo [MISSING] PrismaUI_F4\resources
    set /a FAIL_COUNT+=1
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\misc" (
    echo [OK] PrismaUI_F4\misc
) else (
    echo [MISSING] PrismaUI_F4\misc
    set /a FAIL_COUNT+=1
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\icons" (
    echo [OK] PrismaUI_F4\icons
) else (
    echo [MISSING] PrismaUI_F4\icons
    set /a FAIL_COUNT+=1
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\assets" (
    echo [OK] PrismaUI_F4\assets
) else (
    echo [MISSING] PrismaUI_F4\assets
    set /a FAIL_COUNT+=1
)

echo.
echo Files:
if exist "!DEPLOY_PATH!\F4SE\plugins\PrismaUI_F4.dll" (
    echo [OK] PrismaUI_F4.dll
) else (
    echo [MISSING] PrismaUI_F4.dll
    set /a FAIL_COUNT+=1
)

for %%f in (AppCore.dll Ultralight.dll UltralightCore.dll WebCore.dll) do (
    if exist "!DEPLOY_PATH!\PrismaUI_F4\libs\%%f" (
        echo [OK] %%f
    ) else (
        echo [MISSING] %%f
        set /a FAIL_COUNT+=1
    )
)

for %%f in (cacert.pem icudt67l.dat) do (
    if exist "!DEPLOY_PATH!\PrismaUI_F4\resources\%%f" (
        echo [OK] %%f
    ) else (
        echo [MISSING] %%f
        set /a FAIL_COUNT+=1
    )
)

if exist "!DEPLOY_PATH!\PrismaUI_F4\misc\cursor.png" (
    echo [OK] cursor.png
) else (
    echo [MISSING] cursor.png
    set /a FAIL_COUNT+=1
)

echo.
if !FAIL_COUNT! equ 0 (
    echo ✓ All present
) else (
    echo ✗ !FAIL_COUNT! missing
)

echo.
pause
