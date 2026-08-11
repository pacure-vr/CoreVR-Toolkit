@echo off
setlocal enabledelayedexpansion

echo === CoreVR Toolkit Windows build script ===

if not exist build mkdir build
pushd build
echo Running CMake (x64)...
cmake -A x64 ..
if errorlevel 1 (
    echo CMake configuration failed.
    popd
    exit /b 1
)
echo Building Release...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)
popd

REM copy compiled pybind module into src/ui
echo Copying corevr_bridge module into src/ui
set SRC_UI=src\ui
if exist build\Release\corevr_bridge*.pyd (
    for %%f in (build\Release\corevr_bridge*.pyd) do (
        copy /Y "%%f" "%SRC_UI%\"
    )
) else (
    REM try build\corevr_bridge*.pyd
    if exist build\corevr_bridge*.pyd (
        for %%f in (build\corevr_bridge*.pyd) do (
            copy /Y "%%f" "%SRC_UI%\"
        )
    )
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

echo Build finished. Output in dist_release\Windows\
endlocal
exit /b 0
