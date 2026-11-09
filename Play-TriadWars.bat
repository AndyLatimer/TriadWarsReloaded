@echo off
setlocal
set "ROOT=%~dp0"
title Triad Wars Offline Revival

echo ===================================================
echo     TRIAD WARS OFFLINE REVIVAL - LAUNCHER
echo ===================================================
echo.

if exist "%ROOT%launcher\TriadWarsLauncher.exe" (
    start "" "%ROOT%launcher\TriadWarsLauncher.exe"
    exit /b 0
)

echo Iniciando emulador de servidor local...
start "Triad Wars Server" /min python "%ROOT%server\LocalServer\LocalServer\server.py" --host 127.0.0.1 --port 443

timeout /t 2 /nobreak >nul

echo Iniciando cliente Triad Wars...
cd /d "%ROOT%beta"
start "" "%ROOT%beta\TriadWars.exe"
endlocal
