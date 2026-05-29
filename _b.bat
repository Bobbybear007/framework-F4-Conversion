@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cmake --build "E:\F4SE OG\PrismaUI_F4\build\release" --config Release --target PrismaUI_F4
