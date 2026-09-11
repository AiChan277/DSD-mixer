@echo off
:: ============================================================================
:: DSD Mixer - Build Script (MSVC + Ninja + CMake)
:: ============================================================================

set VCVARS="D:\VS2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist %VCVARS% (
    echo [ERROR] Visual Studio Build Tools belum terdeteksi di D:\VS2022\BuildTools.
    echo Silakan jalankan 'install_build_tools.bat' sebagai Administrator terlebih dahulu.
    pause
    exit /b 1
)

echo [1/3] Mengaktifkan MSVC x64 Environment...
call %VCVARS%

echo [2/3] Mengonfigurasi CMake dengan Ninja...
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

if %errorLevel% neq 0 (
    echo [ERROR] Konfigurasi CMake gagal!
    pause
    exit /b %errorLevel%
)

echo [3/3] Membangun DSD Mixer (Release dengan 4 parallel jobs)...
cmake --build build --config Release -- -j 4

if %errorLevel% neq 0 (
    echo [ERROR] Build gagal!
    pause
    exit /b %errorLevel%
)

echo.
echo ============================================================================
echo [SUKSES] DSD Mixer berhasil dikompilasi!
echo Executable: build\DSDMixer_artefacts\Release\DSD Mixer.exe
echo ============================================================================
pause
