@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >/dev/null 2>&1
cmake --build build/release --config Release --target PrismaUI_F4 2>&1
if errorlevel 1 exit /b 1
echo Build OK — deploying...
xcopy /Y /E /I "dist\PrismaUI_F4_1.0.0\PrismaUI_F4" "E:\Modlists\Fallen World Alpha 2\mods\PrismaUI_F4\PrismaUI_F4"
xcopy /Y "dist\PrismaUI_F4_1.0.0\F4SE\plugins\PrismaUI_F4.dll" "E:\Modlists\Fallen World Alpha 2\mods\PrismaUI_F4\F4SE\plugins\"
echo Done.
