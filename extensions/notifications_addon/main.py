import threading
import time
import socket
import pygame
import os
import math
from array import array
import corevr_bridge

class NotificationToast:
    def __init__(self, text, om=None):
        pygame.init()
        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=22050, size=-16, channels=1)
        self.width = 420
        self.height = 110
        self.text = text
        self.title = f'CoreVR_Toast_{int(time.time()*1000)}'
        self.om = om
        # create borderless window
        self.screen = pygame.display.set_mode((self.width, self.height), pygame.NOFRAME)
        pygame.display.set_caption(self.title)
        self.font = pygame.font.SysFont('Segoe UI', 20)
        # precompute button rects
        self.btn_size = 36
        self.btn_margin = 10
        self.btn_discard = pygame.Rect(self.width - self.btn_margin - self.btn_size*2 - 8, self.height - self.btn_margin - self.btn_size, self.btn_size, self.btn_size)
        self.btn_mark = pygame.Rect(self.width - self.btn_margin - self.btn_size, self.height - self.btn_margin - self.btn_size, self.btn_size, self.btn_size)
        # sounds
        self.ping_sound = self._generate_tone(880, 120, 0.2)
        self.click_sound = self._generate_tone(440, 70, 0.3)

    def _generate_tone(self, freq, ms, volume=0.3):
        sample_rate = 22050
        n_samples = int(sample_rate * (ms / 1000.0))
        arr = array('h')
        amplitude = int(32767 * volume)
        for i in range(n_samples):
            t = float(i) / sample_rate
            v = math.sin(2.0 * math.pi * freq * t)
            s = int(v * amplitude)
            arr.append(s)
        try:
            snd = pygame.mixer.Sound(buffer=arr.tobytes())
            return snd
        except Exception:
            return None

    def show(self, duration=5.0):
        start = time.time()
        clock = pygame.time.Clock()
        if self.ping_sound:
            try: self.ping_sound.play()
            except Exception: pass
        # short haptic on arrival (right hand = role 2)
        try:
            if self.om:
                self.om.trigger_haptic_feedback(2, 0.04, 150.0, 0.5)
        except Exception:
            pass
        running = True
        while running and (time.time() - start < duration):
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    mx, my = event.pos
                    if self.btn_discard.collidepoint(mx, my):
                        if self.click_sound: self.click_sound.play()
                        try:
                            if self.om:
                                self.om.trigger_haptic_feedback(2, 0.02, 200.0, 0.6)
                        except Exception:
                            pass
                        # dismiss
                        running = False
                    elif self.btn_mark.collidepoint(mx, my):
                        if self.click_sound: self.click_sound.play()
                        try:
                            if self.om:
                                self.om.trigger_haptic_feedback(2, 0.02, 200.0, 0.6)
                        except Exception:
                            pass
                        # mark read (could trigger action)
                        running = False
            # draw background with rounded corners
            self.screen.fill((0,0,0,0))
            bg = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
            bg.fill((30,30,36, int(255*0.92)))
            pygame.draw.rect(bg, (20,20,28, int(255*0.85)), bg.get_rect(), border_radius=16)
            # inner glass
            inner = pygame.Surface((self.width-10, self.height-10), pygame.SRCALPHA)
            inner.fill((255,255,255,20))
            pygame.draw.rect(inner, (255,255,255,20), inner.get_rect(), border_radius=12)
            self.screen.blit(bg, (0,0))
            self.screen.blit(inner, (5,5))
            # text
            txt = self.font.render(self.text, True, (220,220,230))
            self.screen.blit(txt, (16, 18))
            # buttons
            pygame.draw.rect(self.screen, (200,60,60), self.btn_discard, border_radius=8)
            pygame.draw.rect(self.screen, (60,200,100), self.btn_mark, border_radius=8)
            # icons
            cx = self.btn_discard.centerx
            cy = self.btn_discard.centery
            pygame.draw.line(self.screen, (255,255,255), (cx-6, cy-6), (cx+6, cy+6), 3)
            pygame.draw.line(self.screen, (255,255,255), (cx+6, cy-6), (cx-6, cy+6), 3)
            cx2 = self.btn_mark.centerx
            cy2 = self.btn_mark.centery
            pygame.draw.line(self.screen, (255,255,255), (cx2-6, cy2), (cx2-2, cy2+6), 3)
            pygame.draw.line(self.screen, (255,255,255), (cx2-2, cy2+6), (cx2+8, cy2-6), 3)
            pygame.display.update()
            clock.tick(60)
        try:
            pygame.display.quit()
        except Exception:
            pass

def setup(manager):
    print('[notifications_addon] setup')
    # start a background thread to simulate or connect to Twitch
    def run():
        # Simulate incoming messages every 10s if no real credentials
        while True:
            time.sleep(10)
            msg = 'Simulated message from Twitch/Discord at ' + time.ctime()
            print('[notifications_addon] New msg:', msg)
            try:
                # respect global zen mode setting
                settings_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'settings.json'))
                try:
                    if os.path.exists(settings_path):
                        import json
                        with open(settings_path, 'r') as sf:
                            s = json.load(sf)
                            if s.get('zen_mode', False):
                                # skip showing toasts while in zen mode
                                continue
                except Exception:
                    pass

                om = corevr_bridge.OverlayManager()
                key = f'corevr.toast.{int(time.time())}'
                om.create_overlay(key, 'Toast')
                om.set_overlay_alpha(0.85)
                om.set_overlay_curvature(0.02)
                # attach overlay to right-hand (role=2) with slight offset above back of hand
                om.attach_to_wrist(2, 0.08, 0.02, 0.02)

                toast = NotificationToast(msg, om=om)
                # start toast in thread
                t = threading.Thread(target=toast.show, args=(5.0,), daemon=True)
                t.start()
                # give window time to appear
                time.sleep(0.4)
                om.set_target_window_by_title(toast.title)
                om.show_overlay()
                # arrival haptic (right hand)
                try:
                    om.trigger_haptic_feedback(2, 0.05, 150.0, 0.6)
                except Exception:
                    pass
                # sleep until toast should be gone
                time.sleep(5.5)
                try:
                    om.hide_overlay()
                except Exception:
                    pass
            except Exception as e:
                print('[notifications_addon] error showing toast', e)

    t = threading.Thread(target=run, daemon=True)
    t.start()

def teardown(manager):
    print('[notifications_addon] teardown')
