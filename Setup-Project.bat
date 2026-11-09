@echo off
setlocal enabledelayedexpansion
title Triad Wars Reloaded - Setup Automatizado
set "ROOT=%~dp0"

echo =====================================================================
echo           TRIAD WARS RELOADED - CONFIGURACION AUTOMATICA
echo =====================================================================
echo.

echo [1/5] Comprobando instalacion de Python...
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python no fue encontrado en el PATH. Por favor instala Python 3.9 o superior.
    pause
    exit /b 1
)
echo       Python detectado correctamente.

echo [2/5] Comprobando carpeta beta del juego...
if not exist "%ROOT%beta\TriadWars.exe" (
    echo [AVISO] No se encontro TriadWars.exe en "%ROOT%beta\".
    echo         Por favor, copia los archivos de tu instalacion original de Triad Wars
    echo         (archivos .big, Data/, TriadWars.exe, etc.) en la carpeta 'beta'.
    echo.
) else (
    echo       Archivos originales de Triad Wars detectados en 'beta\'.
)

echo [3/5] Configurando UserOptions.dat (Servidor Localhost e Idioma Espanol)...
if exist "%ROOT%beta\UserOptions.dat" (
    python "%ROOT%server\LocalServer\LocalServer\useroptions.py" set Environment localhost >nul 2>&1
    python "%ROOT%server\LocalServer\LocalServer\useroptions.py" set Language Spanish >nul 2>&1
    echo       UserOptions.dat configurado (Environment=localhost, Language=Spanish).
) else (
    echo       Aviso: UserOptions.dat no encontrado todavia. Se configurara al anadir la beta.
)

echo [4/5] Aplicando parche de estabilidad de sombras (Spot-shadow crash fix)...
if exist "%ROOT%beta\TriadWars.exe" (
    python "%ROOT%server\LocalServer\LocalServer\tools\patch_exe.py" apply >nul 2>&1
    echo       Parche de sombras aplicado con exito a TriadWars.exe.
)

echo [5/5] Comprobando lanzador independiente (TriadWarsLauncher.exe)...
if not exist "%ROOT%launcher\TriadWarsLauncher.exe" (
    echo       Compilando lanzador C#...
    C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /out:"%ROOT%launcher\TriadWarsLauncher.exe" /platform:x64 /reference:System.Windows.Forms.dll,System.Drawing.dll,System.dll "%ROOT%launcher\src\Launcher.cs" >nul 2>&1
    copy /Y "%ROOT%launcher\TriadWarsLauncher.exe" "%ROOT%TriadWarsLauncher.exe" >nul 2>&1
)
echo       Lanzador listo.

echo.
echo =====================================================================
echo  Configuracion completada con exito.
echo  Para jugar, ejecuta Play-TriadWars.bat o TriadWarsLauncher.exe.
echo =====================================================================
echo.
pause
