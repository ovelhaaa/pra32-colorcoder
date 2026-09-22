#!/usr/bin/env python3
"""Generate SVG screen mockups for the PRA32 Colorcoder VST redesign.

The geometry and palette mirror juce_plugin/Source/PRA32Theme.h so the mocks
and the implementation stay aligned. Run:

    python docs/vst-mockups/generate_mockups.py
"""

import math
import os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

W, H = 960, 620

# --- palette (mirrors PRA32Theme) -------------------------------------------
CHASSIS      = "#101214"
CHASSIS_EDGE = "#0a0b0c"
PANEL        = "#181a1d"
PANEL_RAISED = "#202327"
PANEL_SUNKEN = "#131518"
SEPARATOR    = "#34383c"
BORDER       = "#2b2f33"
TEXT         = "#e7e0d1"
TEXT2        = "#9a948a"
TEXT_DIM     = "#6a655d"
AMBER        = "#d99a3f"
AMBER_HI     = "#e8a020"
CYAN         = "#5eb7b0"
OLIVE        = "#a8b56a"
BLUE         = "#7f9fd0"
COPPER       = "#c98b6a"
MAUVE        = "#b07fb0"
RED          = "#b84b40"

ACCENT = {"OSC": AMBER, "FILTER": CYAN, "ENVS": OLIVE,
          "MOD": BLUE, "FX": COPPER, "COLOR": MAUVE}

OSC1_WAVES  = ["SAW", "SQR", "TRI", "SIN", "WT", "PLS"]
OSC2_WAVES  = ["SAW", "SQR", "TRI", "SIN", "OSC 1", "NOISE"]
LFO_WAVES   = ["TRI", "SINE", "NOISE", "SAW", "S&H", "SQR"]
MOD_DESTS   = ["P 1+2", "P 1+2", "P 2", "P 2", "CUTOFF", "SHAPE"]
VOICE_MODES = ["POLY", "POLY", "MONO", "MONO", "LGTO P", "LEGATO"]

FONT = "Segoe UI, DejaVu Sans, Verdana, sans-serif"
MONO = "Consolas, DejaVu Sans Mono, Courier New, monospace"


def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def txt(x, y, s, size=11, fill=TEXT, anchor="middle", font=FONT,
        weight="normal", spacing=None, opacity=None):
    extra = ""
    if weight != "normal":
        extra += f' font-weight="{weight}"'
    if spacing is not None:
        extra += f' letter-spacing="{spacing}"'
    if opacity is not None:
        extra += f' opacity="{opacity}"'
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{font}" '
            f'font-size="{size:.1f}" fill="{fill}" text-anchor="{anchor}"{extra}>'
            f'{esc(s)}</text>')


def rect(x, y, w, h, fill, rx=4, stroke=None, sw=1, opacity=None):
    s = f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}"'
    if stroke:
        s += f' stroke="{stroke}" stroke-width="{sw}"'
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def line(x0, y0, x1, y1, stroke, sw=1, opacity=None, cap="round"):
    s = (f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" '
         f'stroke="{stroke}" stroke-width="{sw}" stroke-linecap="{cap}"')
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def circle(cx, cy, r, fill, stroke=None, sw=1, opacity=None):
    s = f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{fill}"'
    if stroke:
        s += f' stroke="{stroke}" stroke-width="{sw}"'
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def screw(cx, cy, r):
    out = [circle(cx + 0.5, cy + 0.5, r, "#000000", opacity=0.4)]
    out.append(circle(cx, cy, r, "#b3ac9a", stroke="#000000", sw=0.6))
    out.append(circle(cx - r * 0.3, cy - r * 0.3, r * 0.35, "#f4f0e6", opacity=0.5))
    out.append(line(cx - r * 0.6, cy - r * 0.6, cx + r * 0.6, cy + r * 0.6, "#3a362f", 0.9))
    return "".join(out)


def pt(cx, cy, r, a_deg):
    a = math.radians(a_deg)
    return cx + r * math.sin(a), cy - r * math.cos(a)


def arc(cx, cy, r, a0, a1, stroke, sw=2, opacity=None):
    x0, y0 = pt(cx, cy, r, a0)
    x1, y1 = pt(cx, cy, r, a1)
    delta = a1 - a0
    large = 1 if abs(delta) > 180 else 0
    sweep = 1 if delta > 0 else 0
    s = (f'<path d="M {x0:.2f} {y0:.2f} A {r:.2f} {r:.2f} 0 {large} {sweep} '
         f'{x1:.2f} {y1:.2f}" fill="none" stroke="{stroke}" stroke-width="{sw}" '
         f'stroke-linecap="round"')
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def led(cx, cy, on, accent, r=2.6):
    out = ""
    if on:
        out += circle(cx, cy, r + 2.4, accent, opacity=0.22)
        out += circle(cx, cy, r, accent)
        out += circle(cx, cy, r * 0.45, "#f2efe6")
    else:
        out += circle(cx, cy, r, "#2a2d30")
    return out


START_A, END_A = -135.0, 135.0


def knob(cx, cy, r, prop, label, value, accent, bipolar=False,
         stepped=0, disabled=False):
    """Vintage bakelite/chrome knob with an engraved scale on the panel."""
    out = []
    start = START_A
    a = start + (END_A - START_A) * max(0.0, min(1.0, prop))

    ticks = 10
    for i in range(ticks + 1):
        t = i / ticks
        aa = start + (END_A - START_A) * t
        major = (i % 5 == 0)
        centre_tick = bipolar and i == ticks // 2
        inner = r * 0.86
        outer = r * 0.99
        if centre_tick:
            inner = r * 0.74
            outer = r * 1.02
        x0, y0 = pt(cx, cy, inner, aa)
        x1, y1 = pt(cx, cy, outer, aa)
        col = accent if centre_tick else ("#8d9296" if major else "#5b5f63")
        out.append(line(x0, y0, x1, y1, col,
                        1.8 if centre_tick else (1.3 if major else 1.0)))

    # Drop shadow + fluted bakelite skirt.
    out.append(circle(cx, cy + 2.5, r, "#000000", opacity=0.35))
    out.append(circle(cx, cy, r, "url(#bakelite)", stroke="#000000", sw=1.0))
    flutes = 22
    for i in range(flutes):
        aa = i / flutes * 360.0
        x0, y0 = pt(cx, cy, r * 0.80, aa)
        x1, y1 = pt(cx, cy, r * 0.97, aa)
        out.append(line(x0, y0, x1, y1, "#000000", 1.6, opacity=0.30))

    # Domed body + chrome step.
    body = r * 0.74
    out.append(circle(cx, cy, body, "url(#bakelite)", stroke="#4a463f", sw=1.3))
    out.append(circle(cx - body * 0.25, cy - body * 0.35, body * 0.5,
                      "#ffffff", opacity=0.09))

    # Ivory pointer.
    px, py = pt(cx, cy, r * 0.90, a)
    out.append(line(cx, cy + 1.0, px, py + 1.0, "#000000", 3.0, opacity=0.45))
    out.append(line(cx, cy, px, py, "#e7e0d1", 2.2))
    out.append(circle(px, py, 1.6, "#e7e0d1"))

    # Chrome centre cap.
    cap = body * 0.30
    out.append(circle(cx, cy, cap, "url(#chrome)", stroke="#4a463f", sw=0.8))
    out.append(circle(cx - cap * 0.3, cy - cap * 0.35, cap * 0.35,
                      "#ffffff", opacity=0.45))

    # Engraved caption + value.
    out.append(txt(cx, cy - r - 4, label.upper(), 9.5, "#000000", weight="600"))
    out.append(txt(cx, cy - r - 5, label.upper(), 9.5, TEXT2, weight="600"))
    out.append(txt(cx, cy + r * 0.98 + 12, value, 11,
                   TEXT if not disabled else TEXT_DIM, font=MONO))
    return "".join(out)


def pushbank(x, y, w, h, label, labels, active, accent, cols=0):
    """Interlocked push-button bank. cols=0 => single row, else wraps 3x2 etc."""
    out = []
    name_h = 16
    out.append(txt(x + w / 2, y + name_h - 5, label.upper(), 9.5, "#000000", weight="600"))
    out.append(txt(x + w / 2, y + name_h - 6, label.upper(), 9.5, TEXT2, weight="600"))

    well = (x + 2, y + name_h, w - 4, h - name_h - 2)
    out.append(rect(*well, "#0a0c0e", rx=3, stroke="#000000"))
    inner = (well[0] + 3, well[1] + 3, well[2] - 6, well[3] - 6)

    n = len(labels)
    c = cols if 0 < cols < n else n
    rws = (n + c - 1) // c
    seg_w = inner[2] / c
    seg_h = inner[3] / rws

    for i, text in enumerate(labels):
        sx = inner[0] + (i % c) * seg_w
        sy = inner[1] + (i // c) * seg_h
        seg = (sx + 1, sy + 1, seg_w - 2, seg_h - 2)
        on = (i == active)
        if on:
            out.append(rect(*seg, accent, rx=3))
            out.append(line(seg[0] + 3, seg[1] + 1, seg[0] + seg[2] - 3, seg[1] + 1,
                            "#000000", 1.4, opacity=0.35))
            col = "#000000"
        else:
            out.append(rect(*seg, PANEL_RAISED, rx=3, stroke=BORDER))
            out.append(line(seg[0] + 3, seg[1] + 1.5, seg[0] + seg[2] - 3, seg[1] + 1.5,
                            "#ffffff", 1.0, opacity=0.08))
            col = TEXT
        out.append(txt(seg[0] + seg[2] / 2, seg[1] + seg[3] / 2 + 3,
                       text.upper(), 7.6 if rws > 1 else 7.8, col, font=MONO))
    return "".join(out)


def toggle(x, y, w, h, label, on, accent):
    out = [txt(x + 6, y + h / 2 + 3, label.upper(), 9.5,
               TEXT if on else TEXT2, anchor="start", weight="600")]
    rw, rh = 36, 18
    rx0 = x + w - rw
    ry0 = y + (h - rh) / 2
    out.append(rect(rx0, ry0, rw, rh, "#0a0c0e", rx=4, stroke="#000000"))
    top = accent if on else "#2a2e33"
    bot = "#8a5a12" if on else "#141619"
    gid = f"rk{int(rx0)}_{int(ry0)}"
    out.append(f'<linearGradient id="{gid}" x1="0%" y1="0%" x2="0%" y2="100%">'
               f'<stop offset="0%" stop-color="{top}"/>'
               f'<stop offset="100%" stop-color="{bot}"/></linearGradient>')
    out.append(rect(rx0 + 2, ry0 + 2, rw - 4, rh - 4, f"url(#{gid})",
                    rx=3, stroke="#000000", sw=1))
    out.append(txt(rx0 + rw / 2, ry0 + rh / 2 + 3, "ON" if on else "OFF", 9,
                   "#000000" if on else TEXT2, font=MONO))
    return "".join(out)


def module(x, y, w, h, title, subtitle, accent):
    out = [rect(x, y, w, h, "url(#plate)", rx=5, stroke="#0a0b0c", sw=1.2)]
    out.append(line(x + 7, y + 1.5, x + w - 7, y + 1.5, "#ffffff", 1, opacity=0.08))
    out.append(line(x + 7, y + h - 1.5, x + w - 7, y + h - 1.5, "#000000", 1.6, opacity=0.45))

    for sx, sy in ((x + 9, y + 9), (x + w - 9, y + 9), (x + 9, y + h - 9), (x + w - 9, y + h - 9)):
        out.append(screw(sx, sy, 3.0))

    lamp_x = x + w - 20
    lamp_y = y + 13
    out.append(rect(lamp_x - 5, lamp_y - 5, 10, 10, "#0a0c0e", rx=2, stroke="#000000"))
    out.append(circle(lamp_x, lamp_y, 3.0, accent, opacity=0.85))

    out.append(txt(x + 18, y + 15, title.upper(), 12.5, "#000000", anchor="start", weight="700"))
    out.append(txt(x + 18, y + 14, title.upper(), 12.5, TEXT, anchor="start", weight="700"))

    if subtitle:
        out.append(txt(x + w - 30, y + 15, subtitle.upper(), 9, TEXT_DIM, anchor="end"))

    out.append(line(x + 10, y + 21, x + w - 10, y + 21, "#000000", 1, opacity=0.4))
    return "".join(out)


def header(active):
    out = [rect(0, 0, W, 54, PANEL)]
    out.append(line(0, 54, W, 54, "#000000", 1, opacity=0.5))
    out.append(line(0, 55, W, 55, "#ffffff", 1, opacity=0.04))
    out.append(txt(16, 26, "PRA32", 20, TEXT, anchor="start", weight="700"))
    out.append(txt(16, 42, "C O L O R C O D E R", 9, AMBER, anchor="start", spacing=1.5))

    # preset browser
    cx = W / 2
    out.append(rect(cx - 200, 12, 400, 30, PANEL_SUNKEN, rx=3, stroke=BORDER))
    out.append(txt(cx - 174, 32, "\u2039", 16, TEXT2, anchor="middle"))
    out.append(txt(cx + 174, 32, "\u203a", 16, TEXT2, anchor="middle"))
    out.append(txt(cx, 27, "08  ETHEREAL PAD", 14, TEXT, font=MONO, weight="bold"))
    out.append(txt(cx, 39, "FACTORY PRESET", 8, TEXT_DIM, spacing=1.2))

    for bx, label in ((W - 300, "LOAD JSON"), (W - 200, "SAVE JSON")):
        out.append(rect(bx, 14, 92, 26, PANEL_RAISED, rx=3, stroke=BORDER))
        out.append(txt(bx + 46, 31, label, 9, TEXT2))
    out.append(rect(W - 96, 14, 56, 26, PANEL_RAISED, rx=3, stroke=ACCENT[active]))
    out.append(txt(W - 68, 31, "KEYS", 9, ACCENT[active]))

    return "".join(out)


def rack_rails():
    out = []
    for x0 in (0, W - 10):
        out.append(rect(x0, 0, 10, H, "#1c1f22"))
        out.append(line(x0 + 0.5, 0, x0 + 0.5, H, "#ffffff", 1, opacity=0.05))
        out.append(line(x0 + 9.5, 0, x0 + 9.5, H, "#000000", 1, opacity=0.6))
    for y in (60, H / 2, H - 60):
        out.append(screw(5, y, 3.0))
        out.append(screw(W - 5, y, 3.0))
    return "".join(out)


def section_bar(active):
    out = []
    x0, y0, h = 12, 62, 32
    n = 6
    seg = (W - 24) / n
    for i, name in enumerate(["OSC", "FILTER", "ENVS", "MOD", "FX", "COLOR"]):
        x = x0 + i * seg
        on = (name == active)
        acc = ACCENT[name]
        out.append(rect(x + 2, y0, seg - 4, h, PANEL_RAISED if on else PANEL,
                        rx=3, stroke=acc if on else BORDER, sw=1.2 if on else 1))
        out.append(led(x + 12, y0 + h / 2, on, acc))
        out.append(txt(x + 22, y0 + h / 2 + 4, name, 12, TEXT if on else TEXT2,
                       anchor="start", weight="600" if on else "normal"))
    return "".join(out)


def keyboard():
    x0, y0, w, h = 14, H - 88, W - 28, 76
    out = [rect(x0, y0, w, h, "url(#plate)", rx=5, stroke="#0a0b0c", sw=1.2)]
    out.append(line(x0 + 7, y0 + 1.5, x0 + w - 7, y0 + 1.5, "#ffffff", 1, opacity=0.08))
    out.append(line(x0 + 7, y0 + h - 1.5, x0 + w - 7, y0 + h - 1.5, "#000000", 1.6, opacity=0.45))

    for sx, sy in ((x0 + 9, y0 + 9), (x0 + w - 9, y0 + 9),
                   (x0 + 9, y0 + h - 9), (x0 + w - 9, y0 + h - 9)):
        out.append(screw(sx, sy, 3.0))

    out.append(txt(x0 + 18, y0 + 15, "KEYBOARD", 9, TEXT2, anchor="start", weight="600"))

    bw, bh = 34, 12
    bx = x0 + w - 32 - (bw + 4) * 2
    for i, lab in enumerate(["OCT -", "OCT +"]):
        out.append(rect(bx + i * (bw + 4), y0 + 8, bw, bh, PANEL_RAISED, rx=3, stroke=BORDER))
        out.append(txt(bx + i * (bw + 4) + bw / 2, y0 + 8 + bh - 3, lab, 7.0, TEXT2, font=MONO))

    out.append(rect(x0 + w - 26, y0 + 8, 12, 12, "#0a0c0e", rx=2, stroke="#000000"))
    out.append(circle(x0 + w - 20, y0 + 14, 3.5, AMBER, opacity=0.85))

    kx, ky, kw_all, kh = x0 + 10, y0 + 24, w - 20, h - 30
    keys = 24
    kw = kw_all / keys
    for i in range(keys):
        x = kx + i * kw
        out.append(rect(x + 0.5, ky, kw - 1, kh, "#e9e5da", rx=1.5,
                        stroke="#3a3e42", sw=0.6))
    for i in [0, 1, 3, 4, 5, 7, 8, 10, 11, 12]:
        x = kx + i * kw + kw * 0.66
        out.append(rect(x, ky, kw * 0.62, kh * 0.6, "#14161a", rx=1.5,
                        stroke="#000000", sw=0.6))
    return "".join(out)


def defs():
    return (
        "<defs>"
        '<radialGradient id="knob" cx="38%" cy="30%" r="78%">'
        '<stop offset="0%" stop-color="#2c3136"/>'
        '<stop offset="55%" stop-color="#1d2125"/>'
        '<stop offset="100%" stop-color="#141619"/>'
        "</radialGradient>"
        '<radialGradient id="bakelite" cx="34%" cy="26%" r="82%">'
        '<stop offset="0%" stop-color="#3a3e44"/>'
        '<stop offset="55%" stop-color="#20242a"/>'
        '<stop offset="100%" stop-color="#0c0e11"/>'
        "</radialGradient>"
        '<radialGradient id="chrome" cx="36%" cy="30%" r="72%">'
        '<stop offset="0%" stop-color="#f4f0e6"/>'
        '<stop offset="55%" stop-color="#8f897d"/>'
        '<stop offset="100%" stop-color="#4a463f"/>'
        "</radialGradient>"
        '<linearGradient id="plate" x1="0%" y1="0%" x2="100%" y2="100%">'
        '<stop offset="0%" stop-color="#262a30"/>'
        '<stop offset="100%" stop-color="#171a1e"/>'
        "</linearGradient>"
        "</defs>"
    )


def svg(body):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
        f'viewBox="0 0 {W} {H}">'
        + defs()
        + rect(0, 0, W, H, CHASSIS, rx=0)
        + body
        + rack_rails()
        + "</svg>\n"
    )


# --- shared page furniture ---------------------------------------------------
def start(name):
    return header(name) + section_bar(name)


def end(show_keyboard=True):
    return keyboard() if show_keyboard else ""


# --- page content ------------------------------------------------------------
def page_osc():
    s = start("OSC")
    s += module(12, 102, 936, 128, "Oscillator 1", "", AMBER)
    s += pushbank(60, 150, 120, 52, "WAVE", OSC1_WAVES, 0, AMBER)
    s += knob(310, 172, 40, 0.18, "SHAPE", "18%", AMBER)
    s += knob(500, 172, 40, 0.0, "MORPH", "SAW", AMBER)
    s += knob(690, 172, 40, 0.10, "DRIFT", "10%", AMBER)
    s += pushbank(812, 150, 128, 52, "SAW MODE", ["STRAIGHT", "CURVED"], 0, AMBER)

    s += module(12, 242, 456, 278, "Oscillator 2", "", AMBER)
    s += pushbank(24, 338, 128, 52, "WAVE", OSC2_WAVES, 0, AMBER)
    s += knob(228, 360, 48, 0.5, "COARSE", "+0 st", AMBER, bipolar=True)
    s += knob(368, 360, 48, 0.5, "FINE", "+0 ct", AMBER, bipolar=True)
    s += txt(240, 470, "coarse +/- 24 st   |   fine in cents", 9, TEXT_DIM, font=MONO)

    s += module(480, 242, 468, 278, "Mixer", "", AMBER)
    s += knob(614, 360, 54, 0.5, "OSC 1 / OSC 2", "50 / 50", AMBER, bipolar=True)
    s += knob(834, 360, 54, 0.5, "NOISE / SUB", "0%", AMBER, bipolar=True)
    return s + end()


def page_filter():
    s = start("FILTER")
    s += module(12, 102, 600, 418, "Filter", "", CYAN)
    s += knob(200, 250, 78, 1.0, "CUTOFF", "19.9 kHz", CYAN)
    s += knob(466, 250, 62, 0.0, "RESONANCE", "0.70 Q", CYAN)
    s += txt(200, 362, "cutoff = 440 \u00b7 2^((v-61)/12)", 9, TEXT_DIM, font=MONO)

    # response graph
    gx, gy, gw, gh = 70, 380, 484, 120
    s += rect(gx, gy, gw, gh, PANEL_SUNKEN, rx=3, stroke=BORDER)
    pts = []
    for i in range(0, 101):
        t = i / 100
        f = 20 * (20000 / 20) ** t
        fc = 19912.0
        m = 1.0 / (1.0 + (f / fc) ** 4) ** 0.5
        x = gx + 8 + t * (gw - 16)
        y = gy + gh - 12 - m * (gh - 30)
        pts.append(f"{x:.1f},{y:.1f}")
    s += f'<polyline points="{" ".join(pts)}" fill="none" stroke="{CYAN}" stroke-width="2" opacity="0.8"/>'
    s += line(gx + 8, gy + gh - 12, gx + gw - 8, gy + gh - 12, SEPARATOR, 1)
    s += txt(gx + 10, gy + 14, "RESPONSE (UI)", 8, TEXT_DIM, anchor="start", spacing=0.8)

    s += module(624, 102, 324, 418, "Shaping", "", CYAN)
    s += pushbank(644, 172, 148, 52, "FILTER TYPE", ["LOW PASS", "HIGH PASS"], 0, CYAN)
    s += knob(872, 200, 44, 0.5, "MOD AMOUNT", "+0", CYAN, bipolar=True)
    s += knob(700, 340, 44, 0.5, "KEY TRACK", "+0.000", CYAN, bipolar=True)
    s += knob(872, 340, 44, 0.5, "BREATH AMT", "+0", CYAN, bipolar=True)
    s += toggle(648, 430, 276, 30, "Release = Decay", False, CYAN)
    return s + end()


def adsr_curve(x, y, w, h, a=0.18, d=0.22, sus=0.65, r=0.30, accent=OLIVE):
    ax = x + a * w * 0.55
    dx = ax + d * w * 0.45
    sy = y + h - 10 - sus * (h - 24)
    rx = dx + w * 0.45
    pts = [f"{x + 6},{y + h - 10}", f"{ax:.1f},{y + 10}",
           f"{dx:.1f},{sy:.1f}", f"{rx:.1f},{y + h - 10}"]
    return (f'<polyline points="{" ".join(pts)}" fill="none" stroke="{accent}" '
            f'stroke-width="2" opacity="0.85"/>'
            + line(x + 6, y + h - 10, x + w - 6, y + h - 10, SEPARATOR, 1))


def page_envs():
    s = start("ENVS")
    s += module(12, 102, 456, 238, "Mod Envelope", "", OLIVE)
    s += rect(28, 126, 424, 84, PANEL_SUNKEN, rx=3, stroke=BORDER)
    s += adsr_curve(28, 126, 424, 84)
    s += knob(80, 268, 40, 0.0, "ATTACK", "0.7 ms", OLIVE)
    s += knob(190, 268, 40, 0.5, "DECAY", "200 ms", OLIVE)
    s += knob(300, 268, 40, 1.0, "SUSTAIN", "100%", OLIVE)
    s += knob(410, 268, 40, 0.5, "RELEASE", "200 ms", OLIVE)

    s += module(480, 102, 456, 238, "Amp Envelope", "", OLIVE)
    s += rect(496, 126, 424, 84, PANEL_SUNKEN, rx=3, stroke=BORDER)
    s += adsr_curve(496, 126, 424, 84, 0.1, 0.3, 0.85, 0.2)
    s += knob(548, 268, 40, 0.0, "ATTACK", "0.7 ms", OLIVE)
    s += knob(658, 268, 40, 0.5, "DECAY", "200 ms", OLIVE)
    s += knob(768, 268, 40, 1.0, "SUSTAIN", "100%", OLIVE)
    s += knob(878, 268, 40, 0.5, "RELEASE", "200 ms", OLIVE)

    s += module(12, 352, 924, 168, "Pitch Mod", "", OLIVE)
    s += knob(360, 438, 46, 0.5, "PITCH MOD", "+0 ct", OLIVE, bipolar=True)
    s += pushbank(516, 410, 176, 52, "EG DEST", MOD_DESTS, 0, OLIVE)
    return s + end()


def page_mod():
    s = start("MOD")
    s += module(12, 102, 660, 418, "LFO", "", BLUE)
    s += pushbank(30, 128, 196, 148, "WAVE", LFO_WAVES, 0, BLUE, cols=3)
    s += knob(330, 196, 46, 0.5, "RATE", "2.70 Hz", BLUE)
    s += knob(470, 196, 46, 0.0, "FADE IN", "0 ms", BLUE)
    s += knob(600, 196, 46, 0.0, "DEPTH", "0%", BLUE)
    s += pushbank(30, 330, 196, 148, "LFO DEST", MOD_DESTS, 0, BLUE, cols=3)
    s += knob(330, 398, 46, 0.5, "CUTOFF AMT", "+0", BLUE, bipolar=True)
    s += knob(470, 398, 46, 0.5, "PITCH AMT", "+0 ct", BLUE, bipolar=True)

    s += module(684, 102, 264, 418, "Performance", "", BLUE)
    s += knob(816, 240, 48, 0.02, "BEND RANGE", "2 st", BLUE)
    s += knob(816, 400, 48, 0.0, "GLIDE TIME", "0 ms", BLUE)
    return s + end()


def page_fx():
    s = start("FX")
    s += module(12, 102, 288, 418, "Chorus", "", COPPER)
    s += knob(156, 190, 46, 1.0, "LEVEL", "100%", COPPER)
    s += knob(156, 330, 46, 0.0, "RATE", "0.01 Hz", COPPER)
    s += knob(156, 460, 42, 0.0, "DEPTH", "+0.0 ms", COPPER)

    s += module(312, 102, 396, 418, "Delay", "", COPPER)
    s += knob(510, 195, 58, 0.5, "TIME", "173.3 ms", COPPER)
    s += pushbank(430, 306, 160, 52, "MODE", ["STEREO", "PING PONG"], 0, COPPER)
    s += knob(420, 460, 40, 0.0, "LEVEL", "0%", COPPER)
    s += knob(600, 460, 40, 0.5, "FEEDBACK", "25%", COPPER)

    s += module(720, 102, 228, 418, "Output", "", COPPER)
    s += knob(834, 210, 46, 0.5, "PAN", "CENTER", COPPER, bipolar=True)
    s += knob(834, 360, 46, 0.5, "AMP GAIN", "-11.9 dB", COPPER)
    s += toggle(744, 450, 180, 30, "EG Amp Mod", False, COPPER)
    return s + end()


def page_color():
    s = start("COLOR")
    s += module(12, 102, 456, 418, "Voice", "", MAUVE)
    s += pushbank(24, 214, 192, 52, "VOICE MODE", VOICE_MODES, 0, MAUVE)
    s += pushbank(240, 214, 168, 52, "VOICE ASSIGN", ["MODE 1", "MODE 2"], 0, MAUVE)
    s += pushbank(168, 396, 176, 52, "BREATH AMP", ["OFF", "QUAD", "LIN"], 0, MAUVE)

    s += module(480, 102, 456, 418, "Character", "", MAUVE)
    s += knob(600, 240, 50, 0.0, "MOD VEL", "0%", MAUVE)
    s += knob(816, 240, 50, 0.0, "AMP VEL", "0%", MAUVE)
    s += knob(708, 420, 50, 0.0, "AT LFO AMT", "0%", MAUVE)
    return s + end()


PAGES = {
    "osc": page_osc,
    "filter": page_filter,
    "envs": page_envs,
    "mod": page_mod,
    "fx": page_fx,
    "color": page_color,
}


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in PAGES.items():
        path = os.path.join(OUT_DIR, f"vst-{name}.svg")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(svg(fn()))
        print("wrote", path)


if __name__ == "__main__":
    main()
