import os
import shutil
import PyInstaller.__main__

DLL_NAMES = [
    "libstdc++-6.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll",
    "openvr_api.dll",
]

SEARCH_PATHS = [
    os.environ.get("W64DEVKIT"),
    os.environ.get("W64DEVKIT_HOME"),
    os.environ.get("W64DEVKIT_BIN"),
    r"C:\w64devkit\bin",
    r"C:\msys64\mingw64\bin",
    r"C:\msys64\usr\bin",
]


def find_dll_source(dll_name, extra_dirs=None):
    extra_dirs = extra_dirs or []
    candidates = [p for p in SEARCH_PATHS if p]
    candidates.extend(extra_dirs)
    candidates.extend(os.environ.get("PATH", "").split(os.pathsep))

    for directory in candidates:
        if not directory:
            continue
        if not os.path.isdir(directory):
            continue
        source_path = os.path.join(directory, dll_name)
        if os.path.isfile(source_path):
            return source_path
    return None


def copy_runtime_dlls(output_dir, extra_dirs=None):
    os.makedirs(output_dir, exist_ok=True)
    copied = []
    missing = []

    for dll_name in DLL_NAMES:
        source = find_dll_source(dll_name, extra_dirs=extra_dirs)
        if source:
            destination = os.path.join(output_dir, dll_name)
            try:
                shutil.copy2(source, destination)
                copied.append(os.path.basename(source))
            except Exception as exc:
                print(f"Error copiando {dll_name}: {exc}")
        else:
            missing.append(dll_name)

    if copied:
        print(f"DLLs copiadas a {output_dir}: {', '.join(copied)}")
    if missing:
        print(f"Advertencia: no se encontraron estas DLLs y no se copiaron: {', '.join(missing)}")


def main():
    proj = os.path.abspath(os.path.dirname(__file__))
    src_ui = os.path.join(proj, "src", "ui")
    main_script = os.path.join(src_ui, "main.py")
    assets_dir = os.path.join(src_ui, "assets")
    dist_root = os.path.join(proj, "dist_release", "Windows")
    output_dir = os.path.join(dist_root, "corevr_toolkit")

    print("=== Iniciando empaquetado con PyInstaller (Python Script) ===")

    args = [
        main_script,
        "--name=corevr_toolkit",
        "--onedir",
        "--console",
        f"--paths={src_ui}",
        f"--paths={proj}",
        f"--distpath={dist_root}",
    ]

    if os.path.exists(assets_dir):
        args.append(f"--add-data={assets_dir};assets")

    PyInstaller.__main__.run(args)
    print("=== ¡Empaquetado completado con éxito! ===")

    extra_dirs = [os.path.join(proj, "data", "openvr", "bin", "win64")]
    copy_runtime_dlls(output_dir, extra_dirs=extra_dirs)


if __name__ == "__main__":
    main()
