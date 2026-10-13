#!/usr/bin/env python3
"""PHOQER brand artwork generator (pixel art, flat voice palettes).

  App icon:  framed seal-sun on the sea -> resources/branding/app/

The editor draws the same app logo procedurally (Source/UI/Logo.cpp); keep the two in step.
The Windows .exe icon is app/phoqer-app-icon.ico (16-256 px, each size drawn for that size); the
macOS/Linux build uses app/phoqer-app-icon-32.png and -256.png (see CMakeLists.txt).

Requires numpy and Pillow:  python3 Tools/branding/phoqer_branding.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'resources', 'branding')

# ------------------------------------------------------------------ palette (matches Source/UI/Style.cpp)
def hexc(h): return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)
def mix(a, b, t): return a + (b - a) * t
def darker(c, k): return c / (1.0 + k)              # juce::Colour::darker
WHITE, DARK = hexc('ffffff'), hexc('0a0a0a')
VOICES = {'burp': ('ff2d55', 'ff9f1c', '12040a'), 'squeal': ('e04bff', '33e0ff', '07031a'), 'groan': ('6f9fbb', 'aeb0d4', '060a14')}
def pal(v): a, s, k = VOICES[v]; return hexc(a), hexc(s), hexc(k)

# ------------------------------------------------------------------ mask helpers
def shift(m, dx, dy):
    out = np.zeros_like(m); h, w = m.shape
    out[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = m[max(-dy, 0):h - max(dy, 0), max(-dx, 0):w - max(dx, 0)]
    return out
def dil8(m):
    d = m.copy()
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)): d |= shift(m, dx, dy)
    return d
def outline(m): return dil8(m) & ~m
def paint(shape, layers):
    img = np.zeros(shape + (4,))
    for m, c in layers:
        if isinstance(c, np.ndarray) and c.ndim == 3: img[m, :3] = c[m]
        else: img[m, :3] = c
        img[m, 3] = 255
    return img
def image(img, scale=1):
    im = Image.fromarray(np.clip(np.round(img), 0, 255).astype(np.uint8), 'RGBA')
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)

# ------------------------------------------------------------------ icon geometry (designed in a 32 x 32 space)
def coords(n):
    k = 32.0 / n
    yy, xx = np.mgrid[0:n, 0:n].astype(float)
    return (xx + 0.5) * k, (yy + 0.5) * k, k
def ell(xx, yy, cx, cy, rx, ry, ang=0.0):
    c, s = np.cos(ang), np.sin(ang); dx, dy = xx - cx, yy - cy
    u, v = dx * c + dy * s, -dx * s + dy * c
    return (u / rx) ** 2 + (v / ry) ** 2 <= 1.0
def box(xx, yy, x0, y0, x1, y1): return (xx >= x0) & (xx < x1) & (yy >= y0) & (yy < y1)
def tile(xx, yy, r=4.5):
    m = box(xx, yy, 0, 0, 32, 32)
    for cx, cy in ((r, r), (32 - r, r), (r, 32 - r), (32 - r, 32 - r)):
        q = ((xx < r) if cx < 16 else (xx > 32 - r)) & ((yy < r) if cy < 16 else (yy > 32 - r))
        m &= ~(q & ((xx - cx) ** 2 + (yy - cy) ** 2 > r * r))
    return m
def bands(voice, yy, edges):
    a, s, k = pal(voice)
    cols = [mix(s, WHITE, 0.55), s, mix(a, WHITE, 0.35), a, darker(a, 0.55)]
    out = np.zeros(yy.shape + (3,)); idx = np.digitize(yy, edges)
    for i, c in enumerate(cols): out[idx == i] = c
    return out

HORIZON = 23.0          # the app icon's waterline, in the 32 x 32 design space

def eroded(m): return m & shift(m, 1, 0) & shift(m, -1, 0) & shift(m, 0, 1) & shift(m, 0, -1)

def icon_frame(T, n):
    """A voice-coloured frame just inside the tile edge: 1 px, 2 px from 32 px up."""
    inner = eroded(T)
    frame = T & ~inner
    if n >= 32: frame |= inner & ~eroded(inner)
    return frame

def whisker_mask(n, cy=18.0, inner=6.0, outer=14.6, spread=1.5):
    """Three angled whiskers a side from 32 px up, two straight ones a side below that."""
    xx, yy, px = coords(n); wl = np.zeros((n, n), bool)
    for side in (-1, 1):
        if n >= 32:
            for y0, slope in ((cy - spread, -0.2), (cy, 0.0), (cy + spread, 0.2)):
                for x in np.linspace(16 + side * inner, 16 + side * outer, 120):
                    i, j = int(x / px), int((y0 + slope * abs(x - 16 - side * inner)) / px)
                    if 0 <= i < n and 0 <= j < n: wl[j, i] = True
        else:
            for dy in (0, 2):
                y = int(cy / px) + dy - 1
                for x in range(int((16 + side * inner) / px), int((16 + side * outer) / px), side):
                    if 0 <= x < n: wl[y, x] = True
    return wl

def sea_layers(voice, xx, yy, px, T):
    """Calm sea below the waterline: dark water, three flat reflection bars, a bright waterline."""
    a, s, k = pal(voice)
    sea = T & (yy >= HORIZON)
    bars = np.zeros_like(T)
    for y, half in ((HORIZON + 1.6, 9.0), (HORIZON + 3.8, 6.0), (HORIZON + 6.0, 3.0)):
        bars |= box(xx, yy, 16 - half, y, 16 + half, y + max(px, 1.0))
    return [(sea, mix(k, s, 0.10)), (bars & sea, mix(a, s, 0.35)), (T & (yy >= HORIZON) & (yy < HORIZON + px), mix(a, s, 0.5))]

def soft_whiskers(img, voice, n, frame, yy, cy=18.0, inner=6.0):
    """Light whiskers blended 65% over whatever is underneath, so they read thin."""
    a, s, k = pal(voice)
    wl = whisker_mask(n, cy, inner) & ~frame & (yy < HORIZON)
    light = mix(s, WHITE, 0.55)
    img[wl, :3] = img[wl, :3] * 0.35 + light * 0.65; img[wl, 3] = 255
    return img

def app_icon_small(voice, n):
    """APP LOGO, small sizes (up to 32 px): a framed tile; the round seal-sun face sits on a calm sea."""
    a, s, k = pal(voice); xx, yy, px = coords(n); big = n >= 32
    T = tile(xx, yy)
    sun = ell(xx, yy, 16, 16.5, 12.5, 12.5) & (yy < HORIZON)
    sun &= ~((yy >= 21.5) & (yy < 21.5 + max(0.9, px)))
    eyes = ell(xx, yy, 11.2, 12.8, 1.7, 2.1) | ell(xx, yy, 20.8, 12.8, 1.7, 2.1)
    shine = (box(xx, yy, 10.2, 11.4, 10.2 + px, 11.4 + px) | box(xx, yy, 19.8, 11.4, 19.8 + px, 11.4 + px)) & big
    muzzle = (ell(xx, yy, 13.4, 18.6, 3.6, 2.8) | ell(xx, yy, 18.6, 18.6, 3.6, 2.8)) & sun
    nose = ell(xx, yy, 16, 16.4, 2.0, 1.3)
    frame = icon_frame(T, n)
    img = paint((n, n), [(T, k), (sun & T, bands(voice, yy, [8, 12, 14.5, 21.5])), (muzzle, mix(s, WHITE, 0.6)),
                         (eyes & sun, DARK), (shine & eyes, WHITE), (nose & sun, DARK)]
                        + sea_layers(voice, xx, yy, px, T) + [(frame, a), (outline(T), DARK)])
    return soft_whiskers(img, voice, n, frame, yy)

def app_icon_large(voice, n):
    """APP LOGO, large sizes (48 px and up): the same seal surfacing, its neck widening into shoulders
    at the waterline so the head no longer reads as a ball."""
    a, s, k = pal(voice); xx, yy, px = coords(n)
    T = tile(xx, yy)
    head = ell(xx, yy, 16, 15.6, 11.4, 11.0)
    t = np.clip((yy - 17.0) / (HORIZON - 17.0), 0, 1)
    body = (head | ((yy >= 17.0) & (np.abs(xx - 16) <= 10.6 + 3.6 * t ** 1.6))) & (yy < HORIZON)
    body &= ~((yy >= 21.6) & (yy < 21.6 + max(0.6, px)))
    k_ = 11.4 / 12.5                                   # the small icon's face, scaled to this head
    eyes = ell(xx, yy, 16 - 4.8 * k_, 15.6 - 3.7 * k_, 1.7 * k_, 2.1 * k_) | ell(xx, yy, 16 + 4.8 * k_, 15.6 - 3.7 * k_, 1.7 * k_, 2.1 * k_)
    shine = (box(xx, yy, 10.0, 11.0, 11.0, 12.0) | box(xx, yy, 19.6, 11.0, 20.6, 12.0)) & eyes
    muzzle = (ell(xx, yy, 16 - 2.6 * k_, 15.6 + 2.1 * k_, 3.6 * k_, 2.8 * k_) | ell(xx, yy, 16 + 2.6 * k_, 15.6 + 2.1 * k_, 3.6 * k_, 2.8 * k_)) & body
    nose = ell(xx, yy, 16, 15.6 - 0.1 * k_, 2.0 * k_, 1.3 * k_)
    frame = icon_frame(T, n)
    img = paint((n, n), [(T, k), (body & T, bands(voice, yy, [7, 11, 13.5, 21.0])), (muzzle, mix(s, WHITE, 0.6)),
                         (eyes & body, DARK), (shine, WHITE), (nose & body, DARK)]
                        + sea_layers(voice, xx, yy, px, T) + [(frame, a), (outline(T), DARK)])
    return soft_whiskers(img, voice, n, frame, yy, cy=18.6, inner=6.4)

def app_icon(voice, n):
    return app_icon_small(voice, n) if n < 48 else app_icon_large(voice, n)

def main():
    app = os.path.join(OUT, 'app')
    os.makedirs(app, exist_ok=True)
    # App icon (burp is the default voice). Windows gets a multi-size .ico with each size drawn for
    # that size; macOS/Linux builds take the two PNGs through JUCE (ICON_SMALL / ICON_BIG).
    sizes = [16, 20, 24, 32, 40, 48, 64]
    frames = [image(app_icon('burp', n)) for n in sizes] + [image(app_icon('burp', 64), 4)]
    frames[-1].save(os.path.join(app, 'phoqer-app-icon.ico'), format='ICO', sizes=[(f.width, f.height) for f in frames],
                    append_images=frames[:-1])
    frames[3].save(os.path.join(app, 'phoqer-app-icon-32.png'))
    frames[-1].save(os.path.join(app, 'phoqer-app-icon-256.png'))
    print('wrote', OUT)

if __name__ == '__main__':
    main()
