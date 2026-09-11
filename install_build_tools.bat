@echo off
:: ============================================================================
:: DSD Mixer - Visual Studio 2022 Build Tools Installer
:: Installs MSVC C++ Workload & Windows SDK to D:\VS2022\BuildTools
:: ============================================================================

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Script ini memerlukan hak Administrator.
    echo Silakan klik kanan 'install_build_tools.bat' dan pilih "Run as administrator".
    echo.
    pause
    exit /b 1
)

echo ============================================================================
echo Memulai instalasi Visual Studio 2022 Build Tools (C++ Workload & Windows SDK)
echo Lokasi instalasi: D:\VS2022\BuildTools
echo ============================================================================

set INSTALLER_SRC="%TEMP%\WinGet\Microsoft.VisualStudio.2022.BuildTools.17.14.40\vs_BuildTools.exe"

if not exist %INSTALLER_SRC% (
    echo Mengunduh installer vs_BuildTools.exe...
    powershell -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile '%TEMP%\vs_BuildTools.exe'"
    set INSTALLER_SRC="%TEMP%\vs_BuildTools.exe"
)

%INSTALLER_SRC% --passive --norestart --installPath "D:\VS2022\BuildTools" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended

echo.
echo ============================================================================
echo [SUKSES] Instalasi Build Tools selesai!
echo ============================================================================
pause
