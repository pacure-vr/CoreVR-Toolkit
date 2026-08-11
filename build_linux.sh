#!/usr/bin/env bash
set -euo pipefail

echo "=== CoreVR Toolkit Linux build script ==="

mkdir -p build
pushd build >/dev/null
echo "Running CMake (Release)..."
cmake -DCMAKE_BUILD_TYPE=Release ..
echo "Building..."
make -j$(nproc)
popd >/dev/null

echo "Copying corevr_bridge shared object into src/ui"
mkdir -p src/ui
SO_FILE=$(find build -maxdepth 3 -type f -name 'corevr_bridge*.so' | head -n1 || true)
if [ -n "$SO_FILE" ]; then
  cp -f "$SO_FILE" src/ui/
else
  echo "Warning: compiled corevr_bridge .so not found in build/"
fi

echo "Copying assets into src/ui/assets"
if [ -d assets ]; then
  mkdir -p src/ui/assets
  cp -r assets/* src/ui/assets/ || true
else
  echo "Warning: assets folder not found to copy."
fi

echo "Running PyInstaller..."
pyinstaller corevr_toolkit.spec --clean

echo "Preparing dist_release/Linux"
mkdir -p dist_release/Linux
if [ -f dist/corevr_toolkit/corevr_toolkit ]; then
  mv -f dist/corevr_toolkit/corevr_toolkit dist_release/Linux/corevr_toolkit
elif [ -f dist/corevr_toolkit ]; then
  mv -f dist/corevr_toolkit dist_release/Linux/corevr_toolkit
else
  echo "Warning: PyInstaller output not found in dist/"
fi

echo "Build finished. Output in dist_release/Linux/"
