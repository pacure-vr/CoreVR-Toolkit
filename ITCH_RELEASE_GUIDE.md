# Release guide for itch.io

This document describes what to package and how to publish builds of CoreVR Toolkit to itch.io using `butler`.

1) Package contents

- `CoreVR-Toolkit.exe` or the compiled binaries for Windows (from `build/Release`): the native executable or launcher that bootstraps the Python runtime and the native module.
- `corevr_bridge` compiled module (`.pyd`/`.dll`) if distributed separately.
- `config/` directory with `.xml` files (`default_layout.xml`, `wrist_menu.xml`, `keyboard_layout.xml`).
- `extensions/` folder with community addons.
- `README.md`, `LICENSE`, `ITCH_RELEASE_GUIDE.md`.
- Optionally bundled `python/` or distribution created with `PyInstaller` (single-file executable) including `requirements.txt` dependencies.

2) Building a distributable ZIP

- Build the native binaries (Visual Studio) and copy necessary DLLs alongside executables.
- Create a folder `dist/Release/` and place in it the executable, `config/`, `extensions/`, and any runtime DLLs.
- Zip the `dist/Release/` folder content into `corevr-toolkit-windows-<version>.zip`.

3) Uploading with Butler

Install `butler` from https://itch.io/docs/butler/. Authenticate with `butler login`.

Use the following commands to push a build to your itch.io project (replace `user/game` and path):

```bash
butler push corevr-toolkit-windows-1.0.zip user/corevr-toolkit:windows
```

Automation tip (CI): add a CI job that runs the build, zips the package, and runs the `butler push` command with an API key stored as a secret.

4) Page structure & tags

- Recommended tags: `steamvr`, `open-source`, `utility`, `virtual-reality`, `overlay`.
- In the page description make clear distinctions between:
  - **Source Code**: the repository (open-source) and how to build it.
  - **Compiled Build**: Windows executable/installer that users can download and run directly (may be paid or donation-ware).

5) Licensing & release notes

- Attach a `CHANGELOG.md` and `RELEASE_NOTES.txt` with each release explaining features and breaking changes.

6) Automated build scripts

Two helper scripts are provided in the repository root to produce distributables for Windows and Linux:

- `build_windows.bat` — runs CMake (x64), builds the native C++ code (Release), copies the compiled `corevr_bridge` Python extension into `src/ui/` and runs PyInstaller using `corevr_toolkit.spec`.
- `build_linux.sh` — runs `cmake` + `make` (Release), copies `corevr_bridge.so` into `src/ui/` and runs `pyinstaller corevr_toolkit.spec`.

Both scripts put final artifacts into `dist_release/Windows/` or `dist_release/Linux/` respectively.

7) Packaging for itch.io

After running the appropriate build script, create a ZIP of the corresponding `dist_release/` folder contents. Example recommended zips:

- Windows: `dist_release/Windows/corevr_toolkit.exe` plus the `config/`, `extensions/`, `README.md`, and required DLLs zipped as `corevr-toolkit-windows-<version>.zip`.
- Linux: `dist_release/Linux/corevr_toolkit` plus `config/`, `extensions/`, `README.md` zipped as `corevr-toolkit-linux-<version>.zip`.

When uploading to itch.io, use `butler push` as shown earlier, e.g.:

```bash
butler push corevr-toolkit-windows-1.0.zip user/corevr-toolkit:windows
butler push corevr-toolkit-linux-1.0.zip user/corevr-toolkit:linux
```

Notes:
- If you prefer a single-file executable on Windows, run PyInstaller with `--onefile` (modify `build_windows.bat` to pass `--onefile` to PyInstaller). The provided `.spec` generates a folder build; using `--onefile` will embed everything into a single `.exe` but may increase startup time.
- Test each build on a clean VM to ensure all runtime dependencies are included.
