@echo off
:: build.bat — compile PrismaUI_F4.dll with xmake.
:: Run setup.bat first if this is a fresh checkout.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars64.bat not found. Install Visual Studio 2022 with C++ workload.
    exit /b 1
)

if not exist "%~dp0build\ultralight-1.4.0\include" (
    echo Ultralight SDK not extracted. Running setup.bat first...
    call "%~dp0setup.bat"
    if errorlevel 1 exit /b 1
)

xmake f -m release -y
if errorlevel 1 exit /b 1
xmake build PrismaUI_F4
