# CoreVR Toolkit

CoreVR Toolkit es un motor de superposición VR (SteamVR) que combina un núcleo nativo en C++ con lógica y orquestación en Python. Soporta:

- OpenVR / SteamVR overlays (`IVROverlay`).
- Renderizado con Direct3D11 y subida de texturas para overlays.
- Puente `pybind11` (`corevr_bridge`) para control desde Python.
- Captura de ventanas en Windows (esqueleto WGC + fallback GDI).
- Raycasting nativo y envío de eventos de ratón desde mandos VR.
- Modo cristal (alpha + curvatura) para overlays.

Directorio principal
- `src/engine/` : Código C++ nativo (OpenVR, D3D11, OverlayManager).
- `src/bridge/` : Bindings `pybind11` (módulo `corevr_bridge`).
- `src/ui/` : Scripts Python (UI, main runner, virtual keyboard).
- `include/` : Cabeceras públicas C++.
- `config/` : Layouts XML de ejemplo.
- `build/` : Directorio de compilación.

Requisitos previos
- Windows 10/11 (para WGC y DirectX 11).
- Visual Studio 2022 con "Desktop development with C++".
- CMake >= 3.15
- Python 3.10+ (3.8+ puede funcionar).
- Steam + SteamVR instalado y ejecutándose para probar overlays en VR.

Instalación de dependencias Python

```powershell
python -m pip install -r requirements.txt
```

Compilación en Windows (Visual Studio)

1. Abre "x64 Native Tools Command Prompt for VS 2022" o PowerShell con VS variables cargadas.
2. Desde la raíz del repositorio:

```powershell
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

Notas sobre OpenVR
- `CMakeLists.txt` usa `FetchContent` para descargar `https://github.com/ValveSoftware/openvr.git` si no encuentra OpenVR en rutas estándar.

Funcionamiento General
- El C++ expone `OverlayManager` que permite crear overlays, asignar texturas DirectX, ajustar `alpha` y `curvature`, anclarlos a controladores (`attach_to_wrist`) y capturar ventanas.
- Python (en `src/ui/main.py`) carga `config/default_layout.xml` y crea overlays, posiciona y controla su ciclo de frames. También puede iniciar el teclado virtual (`src/ui/keyboard.py`) basado en Pygame.

Formato XML (propiedades admitidas)
- `id` (string): Identificador único del panel.
- `name` (string): Nombre legible.
- `window_title` (string): Título de la ventana a capturar (opcional). Se usa WGC cuando está disponible o GDI como fallback.
- `alpha` (float 0.0-1.0): Opacidad del overlay (ej. `0.85`).
- `curvature` (float): Curvatura del overlay (0.0 = plano, 0.1 = ligera curva).
- `glass_mode` (true/false): Indicador visual de modo cristal; actualmente aplica alpha y curvatura y el contenido debe renderizar con canal alfa.
- `attach_to` ("wrist_left" / "wrist_right"): Si definido, el overlay se ancla al controlador correspondiente usando `attach_to_wrist`.
- `x`,`y`,`z` (float): Posición en metros en espacio de seguimiento (si no se usa `attach_to`).

Ejemplo de panel:

```xml
<panel id="main" name="MainPanel" x="0.0" y="1.5" z="-2.0" window_title="Calculator" alpha="0.95" curvature="0.0" glass_mode="false" />
```

Prueba rápida (capturar Notepad o Calculator)

1. Abre la aplicación que quieras capturar (por ejemplo `notepad.exe` o `Calculator`).
2. Edita `config/default_layout.xml` y asigna `window_title="Untitled - Notepad"` (o parte del título). Guarda.
3. Asegúrate de que SteamVR esté en ejecución.
4. Construye el proyecto y ejecuta:

```powershell
cd build
cmake --build . --config Release
python -m pip install -r requirements.txt
python src/ui/main.py
```

Limitaciones y notas avanzadas
- WGC (Windows.Graphics.Capture) se ha esbozado en código: la inicialización WinRT y el intento de crear un `GraphicsCaptureItem` están presentes. La integración completa con `Direct3D11CaptureFramePool` y la transferencia eficiente de frames en VRAM requiere producir un `ID3D11Texture2D` interoperable y enlazar correctamente el frame-pool al dispositivo — esto está parcialmente esbozado y puede necesitar ajustes dependiendo del SDK Windows/VS y las bibliotecas de C++/WinRT instaladas.
- La captura por GDI (`PrintWindow`/`BitBlt`) se mantiene como fallback y funciona para muchas aplicaciones, pero puede devolver contenido en blanco para aplicaciones protegidas o aceleradas por GPU (por eso WGC es preferible).
- El raycasting utiliza `IVROverlay::ComputeOverlayIntersection` desde C++ y mapea coordenadas UV a píxeles para enviar eventos de ratón (`WM_*`). Algunos programas pueden requerir `SendInput` en lugar de `PostMessage` para aceptar entrada.

Contribuciones y siguientes pasos
- Integrar completamente WGC -> D3D11 frame-pool -> Upload a overlay (síntesis final en VRAM).
- Añadir ejemplos de renderizado GPU (shader) y compatibilidad con texturas compartidas para menor latencia.
- Mejorar el teclado virtual con un layout personalizable, soporte para mayúsculas/modificadores y diseño internacional.

Contacto
- Este repositorio es un punto de partida. Si quieres que implemente la integración completa WGC->D3D11 (un poco más de trabajo y pruebas en Windows), dime y lo priorizo.
