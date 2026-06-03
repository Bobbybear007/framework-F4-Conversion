@echo off
:: build.bat — compile the example plugin.
:: Run from this folder or the repo root.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars64.bat not found. Install Visual Studio 2022 with C++ workload.
    exit /b 1
)

xmake f -m release -y
if errorlevel 1 exit /b 1
xmake build
