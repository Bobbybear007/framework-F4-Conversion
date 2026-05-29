@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >/dev/null 2>&1
cmake --build build/release --config Release --target PrismaUI_F4
