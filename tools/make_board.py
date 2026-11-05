#!/usr/bin/env python3
"""Builds assets/ui/board.jpg (2x) from the photo in assets/ui_src/board_photo.webp:
removes the garbled AI lettering, draws clean silkscreen and the trimmer footprints
listed in assets/ui_src/board_layout.json."""
import json, sys
from pathlib import Path
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "ui_src"
S = 2                                   # output scale
W, H = 1870, 1090
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"
SILK = (222, 226, 120)                  # yellow-green silkscreen of the photo

def load():
    im = Image.open(SRC / "board_photo.webp").convert("RGB")
    return im.crop((28, 3, 1977, 1141)).resize((W * S, H * S), Image.LANCZOS)

def inpaint(img, boxes, test):
    """remove lettering inside boxes (panel coords): pixels selected by test(r,g,b) are filled from around them"""
    a = np.array(img); mask = np.zeros(a.shape[:2], np.uint8)
    r, g, b = [a[..., i].astype(int) for i in range(3)]
    sel = test(r, g, b)
    for (x0, y0, x1, y1) in boxes:
        X0, Y0, X1, Y1 = int(x0 * S), int(y0 * S), int(x1 * S), int(y1 * S)
        mask[Y0:Y1, X0:X1] = sel[Y0:Y1, X0:X1] * 255
    mask = cv2.dilate(mask, np.ones((5, 5), np.uint8), iterations=2)
    out = cv2.inpaint(cv2.cvtColor(a, cv2.COLOR_RGB2BGR), mask, 9, cv2.INPAINT_TELEA)
    return Image.fromarray(cv2.cvtColor(out, cv2.COLOR_BGR2RGB))

yellow = lambda r, g, b: (r > 105) & (g > 125) & (b < 170) & (r - b > 25)     # silkscreen ink
silk   = lambda r, g, b: (r > 90) & (g > 155)                                 # pale board lettering
light  = lambda r, g, b: ((r > 105) & (g > 120) & (r - b > 20)) | ((r + g + b) > 480)

def T(x, y):   # positions measured on the first (looser) crop -> this crop
    return ((x - 7.6) * W / 1844.4, (y - 2.8) * H / 1077.2)
def TB(b): return (*T(b[0], b[1]), *T(b[2], b[3]))

def font(sz, bold=True): return ImageFont.truetype(FONT_B if bold else FONT, int(sz * S))

def text(d, xy, s, sz, bold=True, fill=SILK, anchor="mm"):
    d.text((xy[0] * S, xy[1] * S), s, font=font(sz, bold), fill=fill, anchor=anchor)

def rect(d, x0, y0, x1, y1, w=1.2, fill=None, outline=SILK):
    d.rectangle([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill, outline=outline, width=max(1, int(w * S)))

def main():
    img = load()
    B = json.loads((SRC / "board_layout.json").read_text())
    # 1. erase: metal plate labels, middle board silkscreen, bad words on the lower board
    img = inpaint(img, [TB((408, 306, 1720, 338))], light)
    img = inpaint(img, [TB((452, 404, 1550, 734))], silk)
    img = inpaint(img, [TB(b) for b in [(310, 752, 410, 790), (305, 862, 380, 930), (1545, 800, 1585, 870),
                        (1585, 905, 1625, 990), (1050, 985, 1250, 1006), (285, 775, 330, 800)]], light)
    d = ImageDraw.Draw(img, "RGBA")
    # 2. metal strip: one plate per instrument channel
    plates = ["ACCENT", "BASS DRUM", "SNARE", "LOW TOM", "MID TOM", "HI TOM", "RIM SHOT", "CLAP", "COWBELL", "CYMBAL", "OPEN HH", "CLOSED HH"]
    for i, name in enumerate(plates):
        text(d, T(458 + i * 109.7, 322), name, 13, fill=(205, 214, 110))
    # 3. lower board fixes
    text(d, T(345, 876), "STEP NO", 12.5, False); text(d, T(345, 919), "1st PART", 12.5, False)
    text(d, T(360, 764), "PCS-808-SEQ", 11, False)
    # 4. middle board: sections, labels and trimmer footprints
    for sec in B["sections"]:
        x0, y0, x1, y1 = sec["box"]
        rect(d, x0, y0, x1, y1, 1.3)
        tw = d.textlength(sec["name"], font=font(17)) / S
        rect(d, x0 + 8, y0 - 12, x0 + 22 + tw, y0 + 12, 1.3, fill=(30, 120, 62, 255))
        text(d, (x0 + 15, y0), sec["name"], 17, anchor="lm")
    for p in B["pads"]:
        cx, cy, r = p["cx"], p["cy"], p["size"] / 2 + 5
        d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=(196, 182, 140, 255), outline=(120, 105, 70, 255), width=S)
        d.ellipse([(cx - r + 3) * S, (cy - r + 3) * S, (cx + r - 3) * S, (cy + r - 3) * S], fill=(24, 92, 48, 255))
        for k, line in enumerate(p["label"].split("\n")):
            text(d, (cx, cy + r + 9 + k * 12), line, 10.5)
    # 5. the 16 STEP keys and the PART LEDs are live in the plugin: show them unlit here
    a = np.array(img).astype(float)
    for k in range(16):
        x0, x1, y0, y1 = int((399 + k * 72.9) * S), int((461 + k * 72.9) * S), int(902 * S), int(991 * S)
        cap = a[y0:y1, x0:x1]
        lum = cap.mean(axis=2, keepdims=True) / 255.0
        cream = np.array([214.0, 200.0, 172.0])
        warm = (cap[..., 0] - cap[..., 1] > 18)[..., None]           # the key cap and the glow (not the green board)
        a[y0:y1, x0:x1] = np.where(warm, np.clip(lum * 1.15, 0, 1.05) * cream, cap)
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    img = inpaint(img, [(318, 893, 372, 918), (326, 942, 356, 968)], lambda r, g, b: (r > 150) | (g > 170))
    d = ImageDraw.Draw(img, "RGBA")
    for k in range(16):
        cx, cy = (429.5 + k * 72.9) * S, 925 * S
        d.ellipse([cx - 9 * S, cy - 9 * S, cx + 9 * S, cy + 9 * S], fill=(70, 18, 16, 255), outline=(40, 30, 25, 255), width=S)
        d.ellipse([cx - 4 * S, cy - 6 * S, cx, cy - 2 * S], fill=(140, 80, 70, 160))
    d.ellipse([331 * S, 899 * S, 349 * S, 917 * S], fill=(70, 18, 16, 255), outline=(30, 40, 30, 255), width=S)   # 1st PART LED
    d.rectangle([331 * S, 946 * S, 349 * S, 964 * S], fill=(78, 62, 14, 255), outline=(30, 40, 30, 255), width=S)  # 2nd PART LED
    img = inpaint(img, [(300, 966, 410, 996)], light)
    d = ImageDraw.Draw(img, "RGBA")
    text(d, (352, 981), "2nd PART", 12.5, False)
    text(d, (148, 140), "POWER", 11, True, fill=(40, 42, 42))
    img.save(ROOT / "assets" / "ui" / "board.jpg", quality=93, subsampling=0)
    print("board.jpg", img.size)

if __name__ == "__main__":
    main()
