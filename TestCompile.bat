@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  PrismaUI_F4 - Build + Deploy FULL FRAMEWORK PACKAGE (single OG + NG DLL)
REM
REM  The DLL is multi-runtime (COMMONLIB_RUNTIMECOUNT=3): one build runs on
REM  Old-Gen, Next-Gen, and VR. No OG/NG switch needed.
REM
REM  Deploys the complete framework MO2 mod layout under the given mod root:
REM    <root>\F4SE\Plugins\PrismaUI_F4.dll        the plugin
REM    <root>\PrismaUI_F4\libs\*.dll              Ultralight SDK (render engine)
REM    <root>\PrismaUI_F4\resources\*             Ultralight resources (cacert/icu)
REM    <root>\PrismaUI_F4\misc\cursor.png         cursor asset
REM    <root>\PrismaUI_F4\views\*                 framework views
REM    <root>\PrismaUI_F4\icons\*                 icon assets
REM
REM  Usage:
REM    TestCompile.bat "E:\...\mods\PrismaUI_F4"     deploy to mod root
REM    TestCompile.bat                                (prompts for mod root)
REM ============================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_LOG=%SCRIPT_DIR%build.log"
set "PROJ_TARGET=PrismaUI_F4"
set "DLL_NAME=PrismaUI_F4.dll"
set "UL_BIN=%SCRIPT_DIR%build\ultralight-1.4.0\bin"
set "UL_RES=%SCRIPT_DIR%build\ultralight-1.4.0\resources"
set "UL_LIC=%SCRIPT_DIR%build\ultralight-1.4.0\license"
set "PRISMA_LICENSE=%SCRIPT_DIR%LICENSE.md"
set "ASSETS=%SCRIPT_DIR%assets"

REM --- Mod root from argument or prompt ---
if "%~1"=="" (
    cls
    color 0A
    echo.
    echo PrismaUI_F4 - Build + Deploy Full Framework ^(OG + NG^)
    echo.
    echo Enter the MOD ROOT folder ^(the framework package is built beneath it^).
    echo Example: E:\Modlists\Fallen World Alpha 2\mods\PrismaUI_F4
    echo.
    set /p "MOD_ROOT=Mod root: "
) else (
    set "MOD_ROOT=%~1"
)

if "!MOD_ROOT!"=="" (
    color 0C
    echo ERROR: No mod root provided
    exit /b 1
)

if not exist "!MOD_ROOT!" (
    echo Mod root does not exist - creating: !MOD_ROOT!
    mkdir "!MOD_ROOT!" 2>nul
    if not exist "!MOD_ROOT!" (
        color 0C
        echo ERROR: Could not create mod root: !MOD_ROOT!
        exit /b 1
    )
)

cls
color 0A
echo.
echo ============================================================================
echo  PrismaUI_F4 - Build + Deploy Full Framework ^(single OG + NG DLL^)
echo ============================================================================
echo Mod root: !MOD_ROOT!
echo.

if exist "%BUILD_LOG%" del "%BUILD_LOG%"
echo [%date% %time%] === BUILD SESSION STARTED === >> "%BUILD_LOG%"

where xmake >nul 2>&1
if errorlevel 1 (
    color 0C
    echo ERROR: xmake not found in PATH
    exit /b 1
)

if not exist "%SCRIPT_DIR%xmake.lua" (
    color 0C
    echo ERROR: xmake.lua not found in %SCRIPT_DIR%
    exit /b 1
)

cd /d "%SCRIPT_DIR%"

REM ============================================================================
REM  BUILD
REM ============================================================================

echo [1/4] Configuring ^(release, x64^)...
xmake f --plat=windows --arch=x64 -m release >>"%BUILD_LOG%" 2>&1
if errorlevel 1 (
    color 0C
    echo [ERROR] Configuration failed. See build.log
    exit /b 1
)

echo [2/4] Building %PROJ_TARGET%...
xmake build -r "%PROJ_TARGET%" >>"%BUILD_LOG%" 2>&1
if errorlevel 1 (
    color 0C
    echo [ERROR] Build failed. See build.log
    exit /b 1
)

set "DLL_PATH="
for /r "%SCRIPT_DIR%build" %%F in (%DLL_NAME%) do (
    if exist "%%F" set "DLL_PATH=%%F"
)
if not defined DLL_PATH (
    color 0C
    echo [ERROR] DLL not found after build
    exit /b 1
)
for %%F in ("%DLL_PATH%") do set "DLL_SIZE=%%~zF"

REM ============================================================================
REM  VERIFY FRAMEWORK SOURCE FILES EXIST BEFORE DEPLOY
REM ============================================================================

echo [3/4] Verifying framework files...
set "MISSING="
if not exist "%UL_BIN%\AppCore.dll"          set "MISSING=!MISSING! libs\AppCore.dll"
if not exist "%UL_BIN%\Ultralight.dll"       set "MISSING=!MISSING! libs\Ultralight.dll"
if not exist "%UL_BIN%\UltralightCore.dll"   set "MISSING=!MISSING! libs\UltralightCore.dll"
if not exist "%UL_BIN%\WebCore.dll"          set "MISSING=!MISSING! libs\WebCore.dll"
if not exist "%UL_RES%\cacert.pem"           set "MISSING=!MISSING! resources\cacert.pem"
if not exist "%UL_RES%\icudt67l.dat"         set "MISSING=!MISSING! resources\icudt67l.dat"

if defined MISSING (
    color 0C
    echo [ERROR] Ultralight SDK files missing:!MISSING!
    echo The framework will not render without these. SDK expected at:
    echo   %SCRIPT_DIR%build\ultralight-1.4.0\
    exit /b 1
)

REM ============================================================================
REM  DEPLOY FULL PACKAGE (targeted copies only - never deletes the mod folder)
REM ============================================================================

echo [4/4] Deploying full framework package...

set "PLUGINS_DIR=!MOD_ROOT!\F4SE\Plugins"
set "FRAMEWORK_DIR=!MOD_ROOT!\PrismaUI_F4"

if not exist "!PLUGINS_DIR!"           mkdir "!PLUGINS_DIR!"
if not exist "!FRAMEWORK_DIR!\libs"      mkdir "!FRAMEWORK_DIR!\libs"
if not exist "!FRAMEWORK_DIR!\resources" mkdir "!FRAMEWORK_DIR!\resources"
if not exist "!FRAMEWORK_DIR!\misc"      mkdir "!FRAMEWORK_DIR!\misc"
if not exist "!FRAMEWORK_DIR!\icons"     mkdir "!FRAMEWORK_DIR!\icons"
if not exist "!FRAMEWORK_DIR!\views"     mkdir "!FRAMEWORK_DIR!\views"

REM Remove stray DLL from a previous wrong deploy (F4SE root instead of Plugins)
if exist "!MOD_ROOT!\F4SE\%DLL_NAME%" del /Q "!MOD_ROOT!\F4SE\%DLL_NAME%" >nul 2>&1

echo   - Plugin DLL...
copy /Y "!DLL_PATH!" "!PLUGINS_DIR!\%DLL_NAME%" >nul 2>&1
if not exist "!PLUGINS_DIR!\%DLL_NAME%" (
    color 0C
    echo [ERROR] Failed to copy DLL to !PLUGINS_DIR!
    exit /b 1
)

echo   - Ultralight libraries...
copy /Y "%UL_BIN%\AppCore.dll"        "!FRAMEWORK_DIR!\libs\" >nul 2>&1
copy /Y "%UL_BIN%\Ultralight.dll"     "!FRAMEWORK_DIR!\libs\" >nul 2>&1
copy /Y "%UL_BIN%\UltralightCore.dll" "!FRAMEWORK_DIR!\libs\" >nul 2>&1
copy /Y "%UL_BIN%\WebCore.dll"        "!FRAMEWORK_DIR!\libs\" >nul 2>&1

echo   - Ultralight resources...
copy /Y "%UL_RES%\cacert.pem"   "!FRAMEWORK_DIR!\resources\" >nul 2>&1
copy /Y "%UL_RES%\icudt67l.dat" "!FRAMEWORK_DIR!\resources\" >nul 2>&1

echo   - Assets (cursor / views / icons)...
if exist "%ASSETS%\misc\cursor.png"  copy /Y "%ASSETS%\misc\cursor.png" "!FRAMEWORK_DIR!\misc\" >nul 2>&1
if exist "%ASSETS%\views"            xcopy /Y /E /I "%ASSETS%\views\*" "!FRAMEWORK_DIR!\views\" >nul 2>&1
if exist "%ASSETS%\icons"            xcopy /Y /E /I "%ASSETS%\icons\*" "!FRAMEWORK_DIR!\icons\" >nul 2>&1

echo   - Licenses (Prisma UI + Ultralight)...
if not exist "!FRAMEWORK_DIR!\licenses" mkdir "!FRAMEWORK_DIR!\licenses"
if exist "%PRISMA_LICENSE%"     copy /Y "%PRISMA_LICENSE%"     "!FRAMEWORK_DIR!\licenses\PrismaUI-LICENSE.md" >nul 2>&1
if exist "%PRISMA_LICENSE%"     copy /Y "%PRISMA_LICENSE%"     "!MOD_ROOT!\LICENSE.md" >nul 2>&1
if exist "%UL_LIC%\LICENSE.txt" copy /Y "%UL_LIC%\LICENSE.txt" "!FRAMEWORK_DIR!\licenses\Ultralight-LICENSE.txt" >nul 2>&1
if exist "%UL_LIC%\EULA.txt"    copy /Y "%UL_LIC%\EULA.txt"    "!FRAMEWORK_DIR!\licenses\Ultralight-EULA.txt" >nul 2>&1
if exist "%UL_LIC%\NOTICES.md"  copy /Y "%UL_LIC%\NOTICES.md"  "!FRAMEWORK_DIR!\licenses\Ultralight-NOTICES.md" >nul 2>&1

REM --- Final verification of the essential runtime files ---
set "DEPLOY_OK=1"
if not exist "!PLUGINS_DIR!\%DLL_NAME%"                set "DEPLOY_OK=0"
if not exist "!FRAMEWORK_DIR!\libs\UltralightCore.dll" set "DEPLOY_OK=0"
if not exist "!FRAMEWORK_DIR!\resources\icudt67l.dat"  set "DEPLOY_OK=0"
if not exist "!FRAMEWORK_DIR!\licenses\PrismaUI-LICENSE.md"     set "DEPLOY_OK=0"
if not exist "!FRAMEWORK_DIR!\licenses\Ultralight-LICENSE.txt"  set "DEPLOY_OK=0"

if "!DEPLOY_OK!"=="0" (
    color 0C
    echo [ERROR] Deploy verification failed - essential files missing in target
    exit /b 1
)

color 0A
echo.
echo ============================================================================
echo  SUCCESS - Full OG + NG framework package deployed
echo ============================================================================
echo.
echo Plugin:    !PLUGINS_DIR!\%DLL_NAME%  (!DLL_SIZE! bytes)
echo Libs:      !FRAMEWORK_DIR!\libs\      (AppCore, Ultralight, UltralightCore, WebCore)
echo Resources: !FRAMEWORK_DIR!\resources\ (cacert.pem, icudt67l.dat)
echo Assets:    !FRAMEWORK_DIR!\misc, \views, \icons
echo Licenses:  !FRAMEWORK_DIR!\licenses\ (PrismaUI + Ultralight LICENSE/EULA/NOTICES)
echo.

endlocal
exit /b 0
