@echo off
setlocal enabledelayedexpansion

echo === CoreVR Toolkit Windows build script (w64devkit / MinGW) ===

if not exist build mkdir build
pushd build
echo Running CMake with MinGW Makefiles...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo CMake configuration failed.
    popd
    exit /b 1
)

echo Building Release...
cmake --build .
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)
popd

REM copy compiled pybind module into src/ui
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

echo Running PyInstaller...
python -m PyInstaller corevr_toolkit.spec --clean
if errorlevel 1 (
    echo PyInstaller failed.
    exit /b 1
)

echo Preparing dist_release/Windows
if not exist dist_release\Windows mkdir dist_release\Windows

REM find exe produced
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
