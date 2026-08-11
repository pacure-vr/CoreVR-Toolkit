import os
import PyInstaller.__main__

def main():
    proj = os.path.abspath(os.path.dirname(__file__))
    src_ui = os.path.join(proj, "src", "ui")
    main_script = os.path.join(src_ui, "main.py")
    assets_dir = os.path.join(src_ui, "assets")
    dist_dir = os.path.join(proj, "dist_release", "Windows")

    print("=== Iniciando empaquetado con PyInstaller (Python Script) ===")

    # Argumentos para PyInstaller por línea de comandos equivalente
    args = [
        main_script,
        "--name=corevr_toolkit",
        "--onedir",
        "--noconsole",
        f"--paths={src_ui}",
        f"--paths={proj}",
        f"--distpath={dist_dir}",
    ]

    # Agregar assets si existen
    if os.path.exists(assets_dir):
        # En PyInstaller, el formato de add-data para Windows es "origen;destino"
        args.append(f"--add-data={assets_dir};assets")

    # Ejecutar PyInstaller
    PyInstaller.__main__.run(args)
    print("=== ¡Empaquetado completado con éxito! ===")


if __name__ == "__main__":
    main()
