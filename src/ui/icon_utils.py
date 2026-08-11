import os
import io

def load_svg_to_pygame_surface(svg_path, pygame):
    """Try to convert an SVG to a pygame.Surface.
    Requires `cairosvg` to be installed. Returns None on failure.
    """
    try:
        import cairosvg
    except Exception:
        return None
    try:
        # Render SVG to PNG bytes
        png_bytes = cairosvg.svg2png(url=svg_path)
        bio = io.BytesIO(png_bytes)
        surf = pygame.image.load(bio)
        try:
            return surf.convert_alpha()
        except Exception:
            return surf.convert()
    except Exception:
        return None

from src.ui.path_utils import get_resource_path

def set_pygame_window_icon_from_svg(svg_relative_path, pygame):
    """Set the pygame window icon if possible. svg_relative_path is workspace-relative path."""
    try:
        if os.path.isabs(svg_relative_path):
            svg_path = svg_relative_path
        else:
            svg_path = get_resource_path(*svg_relative_path.replace('/', os.sep).split(os.sep))
        if not os.path.exists(svg_path):
            return False
        surf = load_svg_to_pygame_surface(svg_path, pygame)
        if surf is None:
            return False
        try:
            pygame.display.set_icon(surf)
            return True
        except Exception:
            return False
    except Exception:
        return False
