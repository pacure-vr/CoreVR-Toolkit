import os
import sys

try:
    from lxml import etree as ET
except Exception:
    import xml.etree.ElementTree as ET

try:
    import corevr_bridge
except Exception as e:
    corevr_bridge = None
    print("Aviso: no se pudo cargar el módulo nativo:", e)

try:
    from src.ui.icon_utils import set_pygame_window_icon_from_svg
except Exception:
    set_pygame_window_icon_from_svg = None


def read_layout(path):
    tree = ET.parse(path)
    root = tree.getroot()
    panels = []
    for p in root.findall('.//panel'):
        id_ = p.get('id', 'panel')
        name = p.get('name', id_)
        x = float(p.get('x', '0'))
        y = float(p.get('y', '0'))
        z = float(p.get('z', '0'))
        opacity = float(p.get('opacity', '1'))
        scale = float(p.get('scale', '1'))
        attach_to = p.get('attach_to', 'world')
        lock = p.get('lock', 'false').lower() in ('1','true','yes')
        panels.append({'id': id_, 'name': name, 'x': x, 'y': y, 'z': z, 'opacity': opacity, 'scale': scale, 'attach_to': attach_to, 'lock': lock})
    return panels


def main():
    # Initialize extension manager early
    from src.ui.extension_manager import ExtensionManager
    ext_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'extensions')
    ext_mgr = ExtensionManager(extensions_dir=os.path.normpath(ext_dir))
    ext_mgr.load_extensions()

    cfg = os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'default_layout.xml')
    cfg = os.path.normpath(cfg)
    if not os.path.exists(cfg):
        print('Config not found:', cfg)
        return
    panels = read_layout(cfg)
    # Also load wrist menu if present
    wrist_cfg = os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'wrist_menu.xml')
    wrist_cfg = os.path.normpath(wrist_cfg)
    if os.path.exists(wrist_cfg):
        wrist_panels = read_layout(wrist_cfg)
        # ensure attach_to property is preserved from xml
        for p in wrist_panels:
            panels.append(p)

    # Allow extensions to register additional panels
    for p in ext_mgr.get_panels():
        panels.append(p)
    print('Panels:', panels)

    if corevr_bridge:
        managers = []
        # helper to create overlays from panels
        def build_overlays(panel_list):
            nonlocal managers
            # hide existing
            for item in managers:
                try:
                    item['mgr'].hide_overlay()
                except Exception:
                    pass
            managers = []
            for p in panel_list:
                key = f"corevr.{p['id']}"
                try:
                    mgr = corevr_bridge.OverlayManager()
                    mgr.create_overlay(key, p.get('name', p['id']))
                    mgr.set_overlay_position(p.get('x', 0.0), p.get('y', 0.0), p.get('z', 0.0))
                    # handle attach_to
                    at = p.get('attach_to', 'world')
                    ox = float(p.get('x', 0.0))
                    oy = float(p.get('y', 0.0))
                    oz = float(p.get('z', 0.0))
                    if at == 'left_hand':
                        try:
                            mgr.attach_to_wrist(1, ox, oy, oz)
                        except Exception:
                            pass
                    elif at == 'right_hand':
                        try:
                            mgr.attach_to_wrist(2, ox, oy, oz)
                        except Exception:
                            pass
                    elif at == 'hmd':
                        try:
                            mgr.attach_to_hmd(ox, oy, oz)
                        except Exception:
                            pass
                    # 'world' remains absolute position
                    # Apply glass parameters if provided
                    alpha = float(p.get('alpha', 1.0))
                    curvature = float(p.get('curvature', 0.0))
                    if alpha < 1.0:
                        mgr.set_overlay_alpha(alpha)
                    if curvature != 0.0:
                        mgr.set_overlay_curvature(curvature)

                        # lock state
                        if p.get('lock', False):
                            try:
                                mgr.set_locked(True)
                            except Exception:
                                pass

                    wt = p.get('window_title', '')
                    if wt:
                        try:
                            mgr.set_target_window_by_title(wt)
                            print(f"Capturing window title '{wt}' into overlay")
                        except Exception as e:
                            print('Warning: set_target_window_by_title failed:', e)

                    mgr.show_overlay()
                    managers.append({'panel': p, 'mgr': mgr})
                    print(f"Overlay '{p.get('name', p['id'])}' shown at ({p.get('x',0)}, {p.get('y',0)}, {p.get('z',0)})")
                except Exception as e:
                    print('Error creating/showing overlay:', e)

        build_overlays(panels)

        # Main render loop (approx 60 FPS)
        import time
        # load settings.json
        import json
        settings_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'settings.json'))
        settings = {'taskbar_enabled': True, 'laser_enabled': True, 'lock_panels': False}
        try:
            if os.path.exists(settings_path):
                with open(settings_path, 'r') as f:
                    settings.update(json.load(f))
        except Exception:
            pass

        # taskbar state
        taskbar_mgr = None
        taskbar_alpha = 0.0
        taskbar_target_alpha = 0.0
        # controller polling manager (for double-tap/shortcuts)
        ctrl_mgr = corevr_bridge.OverlayManager()

        # zen mode state
        zen_mode = settings.get('zen_mode', False)
        zen_shortcut_enabled = settings.get('zen_shortcut_enabled', True)
        # init pygame mixer for sounds
        try:
            import pygame
            if not pygame.mixer.get_init():
                pygame.mixer.init()
            # try to set window icon for any future pygame windows
            try:
                if set_pygame_window_icon_from_svg:
                    set_pygame_window_icon_from_svg('assets/icons/app_icon.svg', pygame)
            except Exception:
                pass
        except Exception:
            pygame = None

        def play_swoosh():
            try:
                if not pygame: return
                # simple sweep tone
                sample_rate = 22050
                duration_ms = 220
                n = int(sample_rate * (duration_ms/1000.0))
                import math
                from array import array
                arr = array('h')
                for i in range(n):
                    t = i / sample_rate
                    # freq sweep 400 -> 1200
                    freq = 400 + 800 * (i / n)
                    v = math.sin(2.0 * math.pi * freq * t) * 0.2
                    arr.append(int(v * 32767))
                snd = pygame.mixer.Sound(buffer=arr.tobytes())
                snd.play()
            except Exception:
                pass
        try:
            # Optional: start virtual keyboard if keyboard layout exists
            kb_cfg = os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'keyboard_layout.xml')
            kb_cfg = os.path.normpath(kb_cfg)
            kb_instance = None
            if os.path.exists(kb_cfg):
                try:
                    from src.ui.keyboard import start_keyboard_in_thread
                    kb_instance = start_keyboard_in_thread()
                    print('Virtual keyboard started')
                except Exception as e:
                    print('Failed to start virtual keyboard:', e)

            # settings overlay state
            settings_overlay = None
            settings_win = None
            try:
                from src.ui.settings_window import start_settings_thread
            except Exception:
                start_settings_thread = None

            while True:
                # handle dynamic extension changes
                if ext_mgr.dirty:
                    panels = read_layout(cfg)
                    if os.path.exists(wrist_cfg):
                        wrist_panels = read_layout(wrist_cfg)
                        for p in wrist_panels:
                            panels.append(p)
                    for p in ext_mgr.get_panels():
                        panels.append(p)
                    build_overlays(panels)
                    ext_mgr.dirty = False

                for item in list(managers):
                    mgr = item['mgr']
                    panel = item['panel']
                    try:
                        mgr.render_test_texture()

                        # poll both controllers for interaction
                        try:
                            left = mgr.poll_controller_intersection(1)
                        except Exception:
                            left = None
                        try:
                            right = mgr.poll_controller_intersection(2)
                        except Exception:
                            right = None

                        # poll physical buttons globally
                        try:
                            ctrl_mgr.poll_physical_buttons()
                            if zen_shortcut_enabled:
                                if ctrl_mgr.is_double_tap_detected(1) or ctrl_mgr.is_double_tap_detected(2):
                                    # toggle zen mode
                                    zen_mode = not zen_mode
                                    settings['zen_mode'] = zen_mode
                                    try:
                                        with open(settings_path, 'w') as f:
                                            import json
                                            json.dump(settings, f)
                                    except Exception:
                                        pass
                                    # audiovisuals
                                    play_swoosh()
                                    try:
                                        # double haptic on both hands
                                        ctrl_mgr.trigger_haptic_feedback(1, 0.04, 200.0, 0.6)
                                        ctrl_mgr.trigger_haptic_feedback(2, 0.04, 200.0, 0.6)
                                    except Exception:
                                        pass
                                    # apply hide/show to all known managers
                                    for it in list(managers):
                                        try:
                                            if zen_mode:
                                                it['mgr'].hide_overlay()
                                            else:
                                                it['mgr'].show_overlay()
                                        except Exception:
                                            pass
                                    # taskbar and settings overlay
                                    try:
                                        if taskbar_mgr:
                                            if zen_mode: taskbar_mgr.hide_overlay()
                                            else: taskbar_mgr.show_overlay()
                                    except Exception:
                                        pass
                                    try:
                                        if settings_overlay:
                                            if zen_mode: settings_overlay.hide_overlay()
                                            else: settings_overlay.show_overlay()
                                    except Exception:
                                        pass
                        except Exception:
                            pass

                        # handle wrist_menu interactions if this is the wrist panel
                        if panel.get('id') == 'wrist_menu':
                            hit = left or right
                            if hit:
                                # pick first non-None
                                hx,hy,hdown = hit
                                # assume texture width 512 and 4 buttons in row
                                col = int((hx / 512.0) * 4)
                                action = None
                                if col == 0:
                                    action = 'open_settings'
                                elif col == 1:
                                    action = 'toggle_keyboard'
                                elif col == 2:
                                    action = 'media_playpause'
                                else:
                                    action = 'show_stats'

                                if hdown:
                                    # perform action
                                    if action == 'open_settings':
                                        # toggle settings window
                                        if settings_win is None and start_settings_thread:
                                            settings_win = start_settings_thread(ext_mgr)
                                            # create VR overlay for settings
                                            try:
                                                settings_overlay = corevr_bridge.OverlayManager()
                                                settings_overlay.create_overlay('corevr.settings', 'Settings')
                                                settings_overlay.set_overlay_position(0.5, 1.3, -1.0)
                                                settings_overlay.set_overlay_alpha(0.95)
                                                settings_overlay.show_overlay()
                                                managers.append({'panel': {'id':'settings_overlay','name':'Settings'}, 'mgr': settings_overlay})
                                            except Exception as e:
                                                print('Failed to create settings overlay:', e)
                                        else:
                                            # close settings
                                            if settings_win:
                                                try:
                                                    settings_win.stop()
                                                except Exception:
                                                    pass
                                                settings_win = None
                                            if settings_overlay:
                                                try:
                                                    settings_overlay.hide_overlay()
                                                except Exception:
                                                    pass
                                                settings_overlay = None
                                    elif action == 'toggle_keyboard':
                                        # toggle keyboard by starting/stopping module
                                        kb_cfg = os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'keyboard_layout.xml')
                                        kb_cfg = os.path.normpath(kb_cfg)
                                        if os.path.exists(kb_cfg):
                                            try:
                                                from src.ui.keyboard import start_keyboard_in_thread
                                                kb = start_keyboard_in_thread()
                                                print('Keyboard toggled')
                                            except Exception as e:
                                                print('Keyboard toggle failed:', e)
                                    elif action == 'media_playpause':
                                        print('Media Play/Pause action (placeholder)')
                                    elif action == 'show_stats':
                                        print('Show stats action (placeholder)')

                    except Exception as e:
                        print('Render error:', e)
                # Taskbar: show when looking down (smooth fade)
                try:
                    if settings.get('taskbar_enabled', True):
                        looking = corevr_bridge.is_hmd_looking_down(40.0)
                    else:
                        looking = False
                    taskbar_target_alpha = 0.95 if looking else 0.0
                    if taskbar_mgr is None and settings.get('taskbar_enabled', True):
                        try:
                            taskbar_mgr = corevr_bridge.OverlayManager()
                            taskbar_mgr.create_overlay('corevr.taskbar', 'Taskbar')
                            taskbar_mgr.attach_to_hmd(0.0, -0.3, -0.6)
                            taskbar_mgr.set_overlay_curvature(0.01)
                            taskbar_mgr.set_overlay_alpha(0.0)
                            taskbar_mgr.show_overlay()
                        except Exception:
                            taskbar_mgr = None
                    if taskbar_mgr:
                        taskbar_alpha += (taskbar_target_alpha - taskbar_alpha) * 0.15
                        try:
                            taskbar_mgr.set_overlay_alpha(taskbar_alpha)
                        except Exception:
                            pass
                except Exception:
                    pass
                time.sleep(1.0 / 60.0)
        except KeyboardInterrupt:
            print('Stopping render loop')


if __name__ == '__main__':
    main()


if __name__ == '__main__':
    main()
