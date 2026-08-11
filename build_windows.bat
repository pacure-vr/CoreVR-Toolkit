@echo off
setlocal enabledelayedexpansion

echo === CoreVR Toolkit Windows build script (w64devkit / MinGW) ===

REM Evitar el error de "dubious ownership" de Git al descargar dependencias con FetchContent
echo Configuring Git safe directory...
git config --global --add safe.directory "*"

REM Asegurar que pybind11 está instalado y obtener su ruta de CMake automáticamente
echo Checking pybind11 and getting its CMake path...
python -m pip install pybind11 >nul 2>&1

for /f "delims=" %%i in ('python -m pybind11 --cmakedir') do set PYBIND11_CMAKE_DIR=%%i
echo Pybind11 CMake dir: !PYBIND11_CMAKE_DIR!

if "!PYBIND11_CMAKE_DIR!"=="" (
    echo ERROR: Could not locate pybind11 CMake directory. Please run: pip install pybind11
    exit /b 1
)

REM Limpiar la carpeta build anterior para evitar conflictos de caché
if exist build (
    echo Cleaning old build cache...
    rmdir /s /q build
)

if not exist build mkdir build
pushd build

echo Running CMake with MinGW Makefiles...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -Dpybind11_DIR="!PYBIND11_CMAKE_DIR!" ..
if errorlevel 1 (
    echo CMake configuration failed.
    popd
    exit /b 1
)

echo Building Release (Downloading OpenVR and compiling)...
cmake --build .
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)
popd

REM Copy compiled pybind module into src/ui
echo Copying corevr_bridge module into src/ui
set SRC_UI=src\ui
if not exist "%SRC_UI%" mkdir "%SRC_UI%"

if exist build\corevr_bridge*.pyd (
    for %%f in (build\corevr_bridge*.pyd) do (
        copy /Y "%%f" "%SRC_UI%\"
    )
) else if exist build\Release\corevr_bridge*.pyd (
    for %%f in (build\Release\corevr_bridge*.pyd) do (
        copy /Y "%%f" "%SRC_UI%\"
    )
) else (
    echo WARNING: corevr_bridge*.pyd not found in build folder.
)

echo Copying assets into src/ui\assets
if exist assets (
    xcopy /E /I /Y assets "%SRC_UI%\assets\" >nul
) else (
    echo Warning: assets folder not found to copy.
)

echo Running PyInstaller through build_toolkit.py...
python build_toolkit.py
if errorlevel 1 (
    echo PyInstaller failed.
    exit /b 1
)

echo Preparing dist_release/Windows
if not exist dist_release\Windows mkdir dist_release\Windows

REM Find exe produced
if exist dist\corevr_toolkit\corevr_toolkit.exe (
    move /Y dist\corevr_toolkit\corevr_toolkit.exe dist_release\Windows\corevr_toolkit.exe
) else if exist dist\corevr_toolkit.exe (
    move /Y dist\corevr_toolkit.exe dist_release\Windows\corevr_toolkit.exe
) else (
    echo WARNING: executable not found in dist\
)

echo Build finished successfully! Output in dist_release\Windows\
endlocal
exit /b 0