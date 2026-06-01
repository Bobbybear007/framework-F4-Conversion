@echo off
:: setup.bat — run once before building to extract the Ultralight 1.4.0 SDK.
:: Only extracts if the include/ folder is not already present.

set SCRIPT_DIR=%~dp0
set SDK_ARCHIVE=%SCRIPT_DIR%external\ultralight-sdk-1.4.0-win-x64.7z
set SDK_DEST=%SCRIPT_DIR%build\ultralight-1.4.0

if exist "%SDK_DEST%\include" (
    echo Ultralight SDK already extracted at build\ultralight-1.4.0
    goto :done
)

echo Extracting Ultralight 1.4.0 SDK...

set SEVENZIP=
if exist "C:\Program Files\7-Zip\7z.exe" set SEVENZIP=C:\Program Files\7-Zip\7z.exe
if not defined SEVENZIP (
    where 7z >nul 2>&1
    if not errorlevel 1 set SEVENZIP=7z
)
if not defined SEVENZIP (
    echo ERROR: 7-Zip not found. Install 7-Zip from https://www.7-zip.org/
    exit /b 1
)

if not exist "%SDK_ARCHIVE%" (
    echo ERROR: Archive not found: %SDK_ARCHIVE%
    exit /b 1
)

mkdir "%SDK_DEST%" 2>nul
"%SEVENZIP%" x "%SDK_ARCHIVE%" -o"%SDK_DEST%" -y
if errorlevel 1 (
    echo ERROR: Extraction failed.
    exit /b 1
)

echo Ultralight SDK extracted to build\ultralight-1.4.0
:done
echo Setup complete. Run build.bat to compile.
