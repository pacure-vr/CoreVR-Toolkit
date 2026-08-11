import pygame
import sys
import threading
import ctypes

try:
    import win32api
    import win32con
except Exception:
    win32api = None

class VirtualKeyboard:
    def __init__(self, width=800, height=300, alpha=200):
        pygame.init()
        self.width = width
        self.height = height
        self.alpha = alpha
        self.screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption('CoreVR Virtual Keyboard')
        self.running = False

    def send_key(self, char):
        # Simple implementation: use win32api keybd_event if available
        if win32api:
            vk = ord(char.upper())
            win32api.keybd_event(vk, 0, 0, 0)
            win32api.keybd_event(vk, 0, win32con.KEYEVENTF_KEYUP, 0)
        else:
            print('Key:', char)

    def draw(self):
        self.screen.fill((30,30,30))
        # Semi-transparent panel
        panel = pygame.Surface((self.width-40, self.height-40), pygame.SRCALPHA)
        panel.fill((255,255,255,self.alpha))
        self.screen.blit(panel, (20,20))

        # Draw simple keys
        font = pygame.font.SysFont('Segoe UI', 24)
        keys = list('QWERTYUIOPASDFGHJKLZXCVBNM')
        per_row = [10,9,7]
        x = 30; y = 40; idx=0
        for r in per_row:
            for i in range(r):
                if idx >= len(keys): break
                k = keys[idx]
                rect = pygame.Rect(x, y, 60, 50)
                pygame.draw.rect(self.screen, (200,200,200,180), rect, border_radius=8)
                txt = font.render(k, True, (10,10,10))
                self.screen.blit(txt, (x+20, y+12))
                x += 66
                idx += 1
            y += 60
            x = 30

    def run(self):
        self.running = True
        clock = pygame.time.Clock()
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                if event.type == pygame.MOUSEBUTTONDOWN:
                    mx,my = event.pos
                    # naive mapping to keys: map x,y to char index
                    idx = (mx // 66) + (my // 60) * 10
                    if idx < 26:
                        ch = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'[idx]
                        self.send_key(ch)
            self.draw()
            pygame.display.update()
            clock.tick(30)

    def stop(self):
        self.running = False
        pygame.quit()

def start_keyboard_in_thread():
    kb = VirtualKeyboard()
    t = threading.Thread(target=kb.run, daemon=True)
    t.start()
    return kb

if __name__ == '__main__':
    kb = VirtualKeyboard()
    kb.run()
