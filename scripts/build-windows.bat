@echo off
setlocal EnableDelayedExpansion

echo ========================================================
echo   DEATH DEALER DRUMS - Windows Build
echo   INFERNO TONES
echo ========================================================

set BUILD_DIR=build\windows
set DELIVERABLES_DIR=DELIVERABLES\Windows

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo [1/3] Configuring with CMake...
cmake -B "%BUILD_DIR%" -S . ^
    -DCMAKE_BUILD_TYPE=Release ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DDDD_COPY_PLUGIN_AFTER_BUILD=ON
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo.
echo [2/3] Building Release...
cmake --build "%BUILD_DIR%" --config Release --parallel
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo [3/3] Done!
echo.
echo Deliverables: %DELIVERABLES_DIR%
echo.
echo ========================================================
echo   BUILD SUCCESSFUL - DEATH DEALER DRUMS
echo ========================================================

endlocal
