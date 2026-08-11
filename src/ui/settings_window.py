import pygame
import threading
import time
import psutil
import json
import os
try:
    import corevr_bridge
except Exception:
    corevr_bridge = None
try:
    from src.ui.icon_utils import set_pygame_window_icon_from_svg
except Exception:
    set_pygame_window_icon_from_svg = None

class SettingsWindow:
    def __init__(self, ext_mgr):
        pygame.init()
        self.ext_mgr = ext_mgr
        self.width = 800
        self.height = 600
        # try to set window icon from SVG if possible
        try:
            if set_pygame_window_icon_from_svg:
                set_pygame_window_icon_from_svg('assets/icons/app_icon.svg', pygame)
        except Exception:
            pass
        self.screen = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption('CoreVR Settings')
        self.running = False
        self.active_tab = 0
        self.tabs = ['Extensions', 'Glass/Panel', 'System']
        # load settings
        self.settings_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'settings.json'))
        self.settings = {'taskbar_enabled': True, 'laser_enabled': True, 'lock_panels': False, 'zen_shortcut_enabled': True, 'zen_mode': False}
        try:
            if os.path.exists(self.settings_path):
                with open(self.settings_path, 'r') as f:
                    self.settings.update(json.load(f))
        except Exception:
            pass

    def draw_tabs(self):
        font = pygame.font.SysFont('Segoe UI', 20)
        x = 10
        for i,t in enumerate(self.tabs):
            color = (200,200,255) if i==self.active_tab else (180,180,180)
            rect = pygame.Rect(x,10,160,36)
            pygame.draw.rect(self.screen, color, rect, border_radius=6)
            txt = font.render(t, True, (10,10,10))
            self.screen.blit(txt, (x+10,14))
            x += 170

    def draw_extensions_tab(self):
        font = pygame.font.SysFont('Segoe UI', 18)
        y = 60
        exts = list(self.ext_mgr.enabled.items())
        for name, enabled in exts:
            color = (100,200,100) if enabled else (200,100,100)
            rect = pygame.Rect(20, y, 600, 36)
            pygame.draw.rect(self.screen, (240,240,240), rect, border_radius=6)
            txt = font.render(f'{name}', True, (10,10,10))
            self.screen.blit(txt, (30, y+8))
            btn_rect = pygame.Rect(640, y, 120, 36)
            pygame.draw.rect(self.screen, color, btn_rect, border_radius=6)
            btn_txt = font.render('Enabled' if enabled else 'Disabled', True, (10,10,10))
            self.screen.blit(btn_txt, (650, y+8))
            y += 50

    def draw_glass_tab(self):
        font = pygame.font.SysFont('Segoe UI', 18)
        txt = font.render('Adjust overlay alpha, curvature and scale from VR settings (controls are placeholders).', True, (10,10,10))
        self.screen.blit(txt, (20,80))

    def draw_system_tab(self):
        font = pygame.font.SysFont('Segoe UI', 18)
        cpu = psutil.cpu_percent()
        mem = psutil.virtual_memory().percent
        txt = font.render(f'CPU: {cpu}%   RAM: {mem}%', True, (10,10,10))
        self.screen.blit(txt, (20,80))
        # toggles
        y = 120
        def draw_toggle(label, key):
            nonlocal y
            font = pygame.font.SysFont('Segoe UI', 16)
            txt = font.render(label, True, (10,10,10))
            self.screen.blit(txt, (20, y))
            state = self.settings.get(key, False)
            rect = pygame.Rect(300, y-4, 80, 28)
            color = (100,200,100) if state else (200,100,100)
            pygame.draw.rect(self.screen, color, rect, border_radius=6)
            st = font.render('On' if state else 'Off', True, (10,10,10))
            self.screen.blit(st, (310, y))
            y += 40

        draw_toggle('Enable Look-Down Taskbar', 'taskbar_enabled')
        draw_toggle('Enable Laser Pointer', 'laser_enabled')
        draw_toggle('Lock Panels (no grab)', 'lock_panels')
        draw_toggle('Enable Zen Mode Shortcut (Double-tap B/Y)', 'zen_shortcut_enabled')

    def run(self):
        self.running = True
        clock = pygame.time.Clock()
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    mx,my = event.pos
                    if my < 60:
                        self.active_tab = mx // 170
                    # handle extension toggles
                    if self.active_tab == 0:
                        y = 60
                        font = pygame.font.SysFont('Segoe UI', 18)
                        exts = list(self.ext_mgr.enabled.items())
                        for idx,(name, enabled) in enumerate(exts):
                            btn_rect = (640, y, 120, 36)
                            bx,by,bw,bh = btn_rect
                            if bx <= mx <= bx+bw and by <= my <= by+bh:
                                # toggle
                                if enabled:
                                    self.ext_mgr.disable_extension(name)
                                else:
                                    self.ext_mgr.enable_extension(name)
                                break
                            y += 50
                    # handle system toggles
                    if self.active_tab == 2:
                        y = 120
                        for key in ('taskbar_enabled','laser_enabled','lock_panels','zen_shortcut_enabled'):
                            bx,by,bw,bh = (300, y-4, 80, 28)
                            if bx <= mx <= bx+bw and by <= my <= by+bh:
                                self.settings[key] = not self.settings.get(key, False)
                                # persist
                                try:
                                    with open(self.settings_path, 'w') as f:
                                        json.dump(self.settings, f)
                                except Exception:
                                    pass
                                # apply laser change immediately
                                if key == 'laser_enabled' and corevr_bridge:
                                    try:
                                        om = corevr_bridge.OverlayManager()
                                        om.set_laser_enabled(self.settings['laser_enabled'])
                                    except Exception:
                                        pass
                                break
                            y += 40

            self.screen.fill((230,230,235))
            self.draw_tabs()
            if self.active_tab == 0:
                self.draw_extensions_tab()
            elif self.active_tab == 1:
                self.draw_glass_tab()
            else:
                self.draw_system_tab()

            pygame.display.update()
            clock.tick(30)

    def stop(self):
        self.running = False
        pygame.quit()

def start_settings_thread(ext_mgr):
    win = SettingsWindow(ext_mgr)
    t = threading.Thread(target=win.run, daemon=True)
    t.start()
    return win
