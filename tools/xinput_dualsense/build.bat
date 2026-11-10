@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl.exe /O2 /LD /MT xinput9_1_0.cpp /link /DEF:xinput9_1_0.def /OUT:XINPUT9_1_0.dll
if %ERRORLEVEL% EQU 0 (
    copy /Y XINPUT9_1_0.dll "..\..\beta\XINPUT9_1_0.dll"
    echo Build and copy succeeded!
) else (
    echo Build failed!
)
