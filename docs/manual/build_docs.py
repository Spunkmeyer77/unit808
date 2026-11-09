#!/usr/bin/env python3
"""Builds docs/UNIT808 Manual.pdf and docs/UNIT808 Update History.pdf in the style of a classic
early-80s drum machine owner's manual (same layout as the Unit 606 documents).

    python3 docs/manual/build_docs.py 0.1.0
"""
import base64, html, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DOCS = HERE.parent
VERSION = sys.argv[1] if len(sys.argv) > 1 else "0.1.0"


def data_uri(p):
    return "data:image/png;base64," + base64.b64encode((HERE / "img" / p).read_bytes()).decode()


BRAND, PANEL, BOARD = data_uri("brand.png"), data_uri("panel.png"), data_uri("board.png")
EXPORT = data_uri("export.png")

# ---------------------------------------------------------------- the segmented "UNIT-808" logotype
SEG = {   # 7-segment style: a top, b upper-right, c lower-right, d bottom, e lower-left, f upper-left, g middle
    "U": "bcdef", "N": "abcef", "I": "bc", "T": "", "-": "g", "6": "acdefg", "0": "abcdef", "8": "abcdefg",
}


def seg_logo(text, h=60, color="#1a1a1a"):
    w, t, gap = h * 0.62, h * 0.16, h * 0.17
    parts, x = [], 0.0
    for ch in text:
        if ch == " ":
            x += w * 0.5
            continue
        cw = w * (0.55 if ch == "-" else (0.35 if ch == "I" else 1.0))
        s = SEG.get(ch, "")
        r = lambda X, Y, W, H: parts.append(f'<rect x="{X:.1f}" y="{Y:.1f}" width="{W:.1f}" height="{H:.1f}" fill="{color}"/>')
        if ch == "T":
            r(x, 0, cw, t); r(x + cw / 2 - t / 2, 0, t, h)
        elif ch == "I":
            r(x + cw / 2 - t / 2, 0, t, h)
        else:
            if "a" in s: r(x, 0, cw, t)
            if "d" in s: r(x, h - t, cw, t)
            if "g" in s: r(x, h / 2 - t / 2, cw, t)
            if "f" in s: r(x, 0, t, h / 2 + t / 2)
            if "e" in s: r(x, h / 2 - t / 2, t, h / 2 + t / 2)
            if "b" in s: r(x + cw - t, 0, t, h / 2 + t / 2)
            if "c" in s: r(x + cw - t, h / 2 - t / 2, t, h / 2 + t / 2)
        x += cw + gap
    return f'<svg viewBox="0 0 {x:.1f} {h}" height="{h}" xmlns="http://www.w3.org/2000/svg">{"".join(parts)}</svg>'



class Doc:
    def __init__(self):
        self.pages = []

    def page(self, body, section=None, contd=False, cls=""):
        n = len(self.pages) + 1
        head = ""
        if section:
            head = f'<div class="sec"><span class="sect">{section}</span>{" <span class=contd>(Cont&rsquo;d)</span>" if contd else ""}</div>'
        num = f'<div class="pnum {"l" if n % 2 == 0 else "r"}">{n}</div>' if section else ""
        self.pages.append(f'<section class="page {cls}">{head}{body}{num}</section>')


def topic(title, body):
    return f'<div class="topic"><div class="tt">{title}</div><div class="tb">{body}</div></div>'


CSS = """
@page { size: 11in 8.5in; margin: 0; }
* { box-sizing: border-box; }
body { margin: 0; font-family: 'Liberation Sans', Arial, sans-serif; color: #1c1c1c; font-size: 9.6pt; line-height: 1.42; }
.page { width: 11in; height: 8.5in; position: relative; padding: 0.95in 0.75in 0.7in 0.75in; page-break-after: always; overflow: hidden;
        background: #fdfdfb; }
.sec { border-bottom: 0.8px solid #444; padding-bottom: 2px; margin-bottom: 14px; letter-spacing: 0.04em; }
.sect { font-size: 13.5pt; font-weight: 400; }
.contd { font-size: 10pt; }
.pnum { position: absolute; bottom: 0.45in; font-family: 'DejaVu Serif', serif; font-style: italic; font-size: 12pt; }
.pnum.l { left: 0.75in; } .pnum.r { right: 0.75in; }
.tb { min-width: 0; }
.topic { display: grid; grid-template-columns: 1.7in minmax(0, 1fr); column-gap: 0.25in; margin-bottom: 16px; }
.tt { font-size: 13pt; font-weight: 300; line-height: 1.2; letter-spacing: 0.01em; }
.cols2 { column-count: 2; column-gap: 0.35in; } .cols3 { column-count: 3; column-gap: 0.3in; }
.cols2 > *, .cols3 > * { break-inside: avoid; }
.cols2 > div { break-inside: avoid-column; }
h4 { font-size: 10pt; letter-spacing: 0.06em; margin: 0 0 6px; font-weight: 700; }
p { margin: 0 0 7px; }
ol { margin: 0 0 6px; padding-left: 1.6em; } ol li { margin-bottom: 7px; padding-left: 0.25em; }
ul { margin: 0 0 6px; padding-left: 1.1em; list-style: none; } ul li { margin-bottom: 4px; position: relative; }
ul li:before { content: "\\25CF"; position: absolute; left: -1.05em; font-size: 7pt; top: 1.5px; }
.note { font-style: italic; display: block; margin: 3px 0 0 0.9em; text-indent: -0.9em; }
.note:before { content: "* "; font-style: normal; }
.cl { font-family: 'DejaVu Sans', sans-serif; font-weight: 400; }
.box { border: 0.8px solid #333; padding: 9px 12px; margin: 6px 0 10px; }
table { border-collapse: collapse; width: 100%; font-size: 9.2pt; margin: 4px 0 8px; }
td, th { text-align: left; vertical-align: top; padding: 3px 8px 3px 0; border-bottom: 0.6px dotted #888; }
th { font-weight: 700; letter-spacing: 0.04em; font-size: 8.8pt; border-bottom: 0.8px solid #333; }
.fig { width: 100%; display: block; } .chart { width: 100%; display: block; }
.figcap { font-weight: 700; margin: 2px 0 4px; }
.dots { display: flex; } .dots .lbl { flex: none; } .dots .fill { flex: 1; border-bottom: 1.4px dotted #777; margin: 0 4px 4px; } .dots .pg { flex: none; }
.toc { width: 4.6in; margin-left: auto; } .toc .dots { margin: 2px 0; } .toc .major { font-weight: 700; letter-spacing: 0.04em; margin-top: 6px; }
.cover .brand { position: absolute; left: 2.1in; top: 1.55in; height: 0.62in; }
.cover .band { position: absolute; left: 2.1in; top: 2.45in; width: 6.3in; height: 0.62in; background:
        repeating-linear-gradient(45deg, #c9c9c5 0 1px, #dededa 1px 3px); padding: 6px 8px; }
.cover .band svg { height: 100%; }
.cover .om { position: absolute; left: 2.1in; top: 3.2in; font-size: 30pt; font-weight: 300; letter-spacing: 0.01em; }
.titlehead { position: absolute; left: 1.05in; right: 1.05in; top: 1.1in; border-bottom: 1px solid #333; display: flex; justify-content: space-between; align-items: flex-end; }
.titlehead .model { font-size: 34pt; font-weight: 700; letter-spacing: 0.01em; line-height: 1; }
.titlehead img { height: 0.34in; margin-bottom: 4px; }
.titlehead2 { position: absolute; left: 1.05in; top: 1.72in; font-size: 15pt; font-weight: 700; }
.inotes { position: absolute; left: 1.3in; top: 5.0in; width: 3.0in; }
.inotes h4 { font-weight: 400; font-size: 11pt; letter-spacing: 0; }
.thumb { position: absolute; right: 1.05in; top: 5.3in; width: 2.4in; filter: grayscale(1) contrast(1.4); opacity: 0.85; }
.back .titlehead { top: auto; bottom: 1.2in; }
.back .titlehead2 { top: auto; bottom: 0.85in; }
.endbrand { position: absolute; left: 50%; top: 60%; transform: translate(-50%, -50%); height: 0.5in; }
.board { width: 100%; border: 0.8px solid #333; display: block; }
"""


def render(doc, title):
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>{title}</title><style>{CSS}</style></head><body>'
            + "".join(doc.pages) + "</body></html>")


def to_pdf(html_text, out_pdf):
    from playwright.sync_api import sync_playwright
    tmp = HERE / "_render.html"
    tmp.write_text(html_text, encoding="utf-8")
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page()
        pg.goto(tmp.as_uri())
        pg.wait_for_timeout(300)
        pg.pdf(path=str(out_pdf), width="11in", height="8.5in", print_background=True, prefer_css_page_size=True)
        b.close()
    tmp.unlink()


def title_page(doc, subtitle, notes_html):
    doc.page(f'''<div class="titlehead"><div class="model">UNIT808</div><img src="{BRAND}"></div>
<div class="titlehead2">{subtitle}</div>
<div class="inotes">{notes_html}</div>
<img class="thumb" src="{PANEL}">''', cls="title")


def cover(doc, what):
    doc.page(f'''<img class="brand" src="{BRAND}">
<div class="band">{seg_logo("UNIT-808", 60)}</div>
<div class="om">{what}</div>''', cls="cover")


def back(doc, subtitle):
    doc.page(f'''<div class="titlehead"><div class="model">UNIT808</div><img src="{BRAND}"></div>
<div class="titlehead2">{subtitle}</div>''', cls="back")


def toc_line(label, pg, major=False):
    return f'<div class="dots{" major" if major else ""}"><span class="lbl">{label}</span><span class="fill"></span><span class="pg">{pg}</span></div>'



# ---------------------------------------------------------------- the panel with lettered callouts (panel 1870 x 1090)
CALLOUTS = [
    ("A", "POWER", " Switch (Inside)", (148, 113), (-60, 70), "r"),
    ("B", "MODE", " Selector", (148, 296), (-60, 250), "r"),
    ("C", "INSTRUMENT-SELECT", " / RHYTHM TRACK", (320, 296), (320, -110), "c"),
    ("D", "CLEAR", " Button", (155, 402), (-60, 405), "r"),
    ("E", "MEASURES AUTO FILL IN", "", (330, 438), (-60, 520), "r"),
    ("F", "SWING", " Control", (184, 610), (-60, 640), "r"),
    ("G", "FLAM", " Control", (346, 645), (-60, 740), "r"),
    ("H", "LEVEL", " Controls", (1075, 244), (1075, -110), "c"),
    ("I", "TONE \u00b7 TUNING \u00b7 SNAPPY \u00b7 DECAY", " Controls", (1460, 360), (1960, 330), "l"),
    ("J", "Sound Selector", " Switches", (1240, 524), (1960, 520), "l"),
    ("K", "MASTER VOLUME", " Control", (1735, 656), (1960, 660), "l"),
    ("L", "TRANSISTOR LABS", " Logo (About)", (1600, 124), (1960, 124), "l"),
    ("M", "BASIC-VARIATION", " Lever", (192, 798), (-60, 840), "r"),
    ("N", "", "Variation Indicators A / B", (192, 852), (-60, 920), "r"),
    ("O", "START/STOP", " Button", (187, 955), (-60, 1010), "r"),
    ("P", "PRE-SCALE", " Switch", (353, 814), (300, 1200), "c"),
    ("Q", "", "1st / 2nd PART Indicators", (349, 940), (300, 1270), "c"),
    ("R", "STEP", " Keys", (990, 1020), (990, 1200), "c"),
    ("S", "I/F-VARIATION", " Lever", (1714, 798), (1960, 800), "l"),
    ("T", "TAP", " (Intro Set / Fill In Trigger)", (1710, 960), (1960, 980), "l"),
]


def panel_figure():
    vx, vy, vw, vh = -960, -200, 3860, 1530
    out = [f'<svg class="fig" viewBox="{vx} {vy} {vw} {vh}" xmlns="http://www.w3.org/2000/svg">',
           f'<image href="{PANEL}" x="0" y="0" width="1870" height="1090"/>',
           '<rect x="0" y="0" width="1870" height="1090" fill="none" stroke="#333" stroke-width="2"/>']
    for letter, bold, plain, (tx, ty), (lx, ly), side in CALLOUTS:
        if letter == "H":
            out.append('<path d="M470 205 V-60 H1680 V205 M1075 -60 V-80" fill="none" stroke="#222" stroke-width="2.4"/>')
        elif letter == "R":
            out.append('<path d="M440 1060 V1130 H1550 V1060 M990 1130 V1150" fill="none" stroke="#222" stroke-width="2.4"/>')
        else:
            out.append(f'<line x1="{tx}" y1="{ty}" x2="{lx}" y2="{ly}" stroke="#222" stroke-width="2.4"/>')
            out.append(f'<circle cx="{tx}" cy="{ty}" r="7" fill="#222"/>')
        anchor = {"l": "start", "r": "end", "c": "middle"}[side]
        off = {"l": 14, "r": -14, "c": 0}[side]
        dy = 52 if side == "c" and ly > 900 else (-14 if side == "c" else 16)
        label = (f'<tspan class="cl">{chr(0x24B6 + ord(letter) - 65)}</tspan>'
                 f' <tspan font-weight="700">{html.escape(bold)}</tspan>{html.escape(plain)}')
        out.append(f'<text x="{lx + off}" y="{ly + dy}" text-anchor="{anchor}" font-size="44">{label}</text>')
    out.append("</svg>")
    return "".join(out)


FIG1 = {"AC": [0, 8], "BD": [0, 6, 10], "SD": [4, 12], "CH": [0, 2, 4, 6, 8, 10, 12, 14]}


def step_chart():
    rows = ["BD", "SD", "CH", "AC"]
    x0, dx, y0, dy = 120, 42, 50, 34
    o = [f'<svg class="chart" viewBox="0 0 {x0 + 16 * dx + 20} {y0 + len(rows) * dy + 30}" xmlns="http://www.w3.org/2000/svg">',
         '<text x="4" y="26" font-size="17">|Step No|\u2192</text>']
    for s in range(16):
        o.append(f'<text x="{x0 + s * dx}" y="26" font-size="17" text-anchor="middle">{s + 1}</text>')
        o.append(f'<line x1="{x0 + s * dx}" y1="{y0 - 12}" x2="{x0 + s * dx}" y2="{y0 + (len(rows) - 1) * dy + 10}" stroke="#555" stroke-width="1.2"/>')
    for r, name in enumerate(rows):
        y = y0 + r * dy
        o.append(f'<text x="40" y="{y + 6}" font-size="17">{name}</text>')
        for s in FIG1[name]:
            o.append(f'<circle cx="{x0 + s * dx}" cy="{y}" r="8" fill="#111"/>')
    for s in (0, 4, 8, 12):
        o.append(f'<path d="M{x0 + s * dx - 7} {y0 + len(rows) * dy + 8} l7 -12 l7 12 z" fill="#111"/>')
    o.append("</svg>")
    return "".join(o)


def circ(letter):
    return f'<span class="cl">{chr(0x24B6 + ord(letter) - 65)}</span>'


C = {k: circ(k) for k in "ABCDEFGHIJKLMNOPQRST"}


def ctl(letter, name, text):
    return f'<p>{C[letter]} <b>{name}</b><br>{text}</p>'


# ================================================================ MANUAL
def manual():
    d = Doc()
    cover(d, "Owner&rsquo;s Manual")
    title_page(d, "Operation Manual", f'''<h4>Important Notes</h4><ul>
<li>UNIT808 keeps everything you write (rhythms, tracks, knobs and switches) inside your DAW project or Live set. Save your project to keep it.</li>
<li>Tempo and transport always follow your DAW.</li>
<li>This manual describes version {VERSION}.</li></ul>''')

    toc = [("INTRODUCTION", 4, True), ("BASIC COURSE", 5, True), ("Installation", 5), ("Front Panel Layout", 6),
           ("Writing Rhythm Patterns", 7), ("Playing Rhythm Patterns", 9), ("Intro / Fill In", 10),
           ("Composing and Playing Rhythm Tracks", 11), ("ADVANCED COURSE", 12, True), ("Control Descriptions", 12),
           ("Step Number, 2nd Part, Clearing, Copy and Paste", 13), ("Pre-Scale, Swing and Flam", 14), ("Sound Controls", 14),
           ("Mute and Solo, Selecting with the Name Windows", 15), ("Playing from MIDI", 16), ("Running in a DAW, Saving, MIDI Mapping", 16),
           ("How Each Voice Is Modelled", 17), ("Inside the Box (Trimmers)", 18), ("Exporting MIDI", 20), ("About Window, Window Size, Troubleshooting", 21), ("Specifications, System Requirements", 22)]
    d.page('<div class="toc"><div class="sec"><span class="sect">TABLE OF CONTENTS</span></div>'
           + "".join(toc_line(t[0], t[1], len(t) > 2) for t in toc) + "</div>")

    d.page('''<div class="cols3">
<p>The Transistor Labs UNIT808 is a computer-controlled Rhythm Composer for your computer, modelled on the 1981 classic.
Its sounds are synthesized, not sampled: the same sound engine as the UNIT 808 Max for Live device,
built from analog-style circuits for every instrument.</p>
<p>Eleven sound channels play sixteen sounds: Bass Drum, Snare Drum, Low, Mid and Hi Tom or Conga, Rim Shot or Claves,
Hand Clap or Maracas, Cow Bell, Cymbal, Open Hi-Hat and Closed Hi-Hat, plus ACcent. Each has its own LEVEL control,
and most have TONE, TUNING, SNAPPY or DECAY controls.</p>
<p>In this manual each bar of music is called a &ldquo;Rhythm Pattern&rdquo;.</p>
<p>UNIT808 memorizes 12 Basic Rhythms and 4 Intro / Fill In patterns. Every one has two variations, A and B, and each
variation has a 1st Part and an optional 2nd Part of up to 16 steps, so a rhythm can be up to 32 steps long. Twelve
Rhythm Tracks chain the rhythms into songs.</p>
<p>The BASIC-VARIATION lever plays A, B or A then B; TAP plays a fill on the next bar, and MEASURES AUTO FILL IN plays
one every 2, 4, 8, 12 or 16 bars.</p>
<p>UNIT808 always plays in time with your DAW: it starts when the song plays and follows the song&rsquo;s tempo.
The SWING control takes the place of the tempo knob, and FLAM sets the spacing of flammed hits.</p>
<p>It runs as an AU, VST3 or LV2 plug-in, or as a standalone application, on macOS, Windows and Linux.</p>
<p>This owner&rsquo;s manual has two courses consisting of a Basic Course and an Advanced Course.</p></div>''', "INTRODUCTION")

    d.page(topic("Installation", '''<div class="cols3">
<h4>macOS</h4><ol><li>Download <b>UNIT808-''' + VERSION + '''-macOS.pkg</b> and open it.</li>
<li>The installer is not signed: macOS blocks it the first time. Open System Settings &rarr; Privacy &amp; Security and
click <b>Open Anyway</b>.</li><li>Click <b>Customize</b> to skip the AU, the VST3 or the application if you like.</li></ol>
<p>Installed to /Library/Audio/Plug-Ins/Components, /Library/Audio/Plug-Ins/VST3 and /Applications.</p>
<h4>Windows</h4><ol><li>Run <b>UNIT808-''' + VERSION + '''-Windows-Setup.exe</b>.</li>
<li>If SmartScreen appears, click <b>More info &rarr; Run anyway</b>.</li></ol>
<p>The VST3 goes to C:\\Program Files\\Common Files\\VST3, the application to the Start menu under Transistor Labs.</p>
<h4>Linux</h4><ol><li>Unpack the <b>.tar.gz</b> and run <b>./install.sh</b>.</li></ol>
<p>VST3 to ~/.vst3, LV2 to ~/.lv2, the application to ~/.local/bin/UNIT808.</p>
<h4>Loading it in Ableton Live</h4><ol><li>Settings &rarr; Plug-Ins: turn on <b>Use Audio Units v2</b> (Mac) or
<b>VST3</b>, then click <b>Rescan</b>.</li><li>In the browser, open Plug-Ins &rarr; Transistor Labs &rarr; <b>UNIT808</b> and
drag it onto a MIDI track.</li><li>Press Play in Live. UNIT808 plays in time with the song.</li></ol></div>'''),
           "BASIC COURSE")

    d.page(f'''<div class="topic" style="margin-bottom:0"><div class="tt">Front Panel Layout</div><div class="tb"><h4>FRONT PANEL LAYOUT</h4></div></div>
<div style="margin: 0 -0.4in">{panel_figure()}</div>''', "BASIC COURSE", True)

    d.page(topic("Writing Rhythm Patterns", f'''<div class="cols2"><div>
<p>We will write the Rhythm Pattern shown in Fig. 1 into Basic Rhythm 1, variation A, 1st Part.</p>
<div class="figcap">Fig. 1</div><div class="box">{step_chart()}</div></div>
<div><h4>PROCEDURE</h4><ol>
<li>Load UNIT808 on a MIDI track (see INSTALLATION).</li>
<li>Set the MODE selector {C["B"]} to MANUAL PLAY and press STEP key {C["R"]} #1: Basic Rhythm 1 is chosen and its indicator lights.
<span class="note">Click or drag towards any printed label around the MODE, INSTRUMENT-SELECT and AUTO FILL IN selectors to turn them there.</span></li>
<li>Set the BASIC-VARIATION lever {C["M"]} to A.</li>
<li>Set the MODE selector {C["B"]} to 1st PART (PATTERN WRITE).</li>
<li>Turn INSTRUMENT-SELECT {C["C"]} to BD (Bass Drum), or simply click the BASS DRUM name window on its channel.
<span class="note">Every name window on the channels (ACCENT, BASS DRUM &hellip; CLS&rsquo;D HIHAT, and LOW CONGA &hellip; MARACAS above) selects its instrument; the selected one has an orange frame.</span></li></ol></div></div>'''), "BASIC COURSE", True)

    d.page(topic("", f'''<div class="cols2"><div><ol start="6">
<li>Press the STEP keys {C["R"]} matching the dots in the score (Fig. 1).
<span class="note">The indicators of the written steps light. Press a lit key again to erase the step.</span>
<span class="note">To write many steps at once, press a key and drag across the others: every key you pass takes the same state as the first one.</span></li>
<li>Repeat 5 &ndash; 6 for SD and CH. Then select AC (ACcent) and press keys #1 and #9.
<span class="note">Every sound on an AC step plays at full level; the ACCENT control sets how much softer the other steps are.</span></li></ol></div>
<div><ol start="8">
<li>Press Play in your DAW. The playing step shows inverted, so you can follow it.</li>
<li>You can also write in real time: while the rhythm runs, press TAP {C["T"]} on the beat. The selected instrument is written on the playing step.</li>
<li>To make a hit a <b>flam</b>, Option/Alt-click its STEP key: the step lights <b>blue</b>. Option/Alt-click again to make it a normal hit.
<span class="note">Flams are per instrument: the snare can flam on a step while the bass drum on the same step does not. FLAM {C["G"]} sets the spacing.</span></li></ol>
<p class="note">You have just written the Rhythm Pattern of Fig. 1.</p>
<div class="box"><b>IMPORTANT NOTES</b><br><i>The BASIC-VARIATION lever chooses which variation you write: A, or B (AB writes A).
The 2nd Part, the number of steps and clearing are described in the ADVANCED COURSE.</i></div></div></div>'''), "BASIC COURSE", True)

    d.page(topic("Playing Rhythm Patterns", f'''<div class="cols2"><h4>PROCEDURE</h4><ol>
<li>Set the MODE selector {C["B"]} to MANUAL PLAY.</li>
<li>Press STEP keys {C["R"]} #1 &ndash; #12 to choose Basic Rhythm 1 &ndash; 12.
<span class="note">While the rhythm runs, the new rhythm waits for the end of the bar (its indicator blinks), then takes over.</span></li>
<li>Choose the variation with the BASIC-VARIATION lever {C["M"]}:<br>A &hellip; variation A, &nbsp;AB &hellip; A then B, &nbsp;B &hellip; variation B
<span class="note">The A and B indicators {C["N"]} show the variation that is playing; the 1st/2nd PART indicators {C["Q"]} show the part.</span></li>
<li>Press Play in your DAW. The running light follows the playing step.</li></ol></div>'''), "BASIC COURSE", True)

    d.page(topic("Intro / Fill In", f'''<div class="cols2"><div>
<p>STEP keys #13 &ndash; #16 hold four Intro / Fill In patterns, written like any rhythm (choose the key in MANUAL PLAY,
then write it in 1st PART).</p><h4>PROCEDURE</h4><ol>
<li>In MANUAL PLAY, press STEP key {C["R"]} #13 &ndash; #16 to choose the fill.</li>
<li>Choose its variation with the I/F-VARIATION lever {C["S"]}.</li>
<li>While the rhythm plays, press TAP {C["T"]} (INTRO SET / FILL IN TRIGGER). The fill plays as the next bar, then the rhythm continues.
<span class="note">The fill&rsquo;s indicator blinks while it waits.</span></li></ol></div>
<div><h4>AUTO FILL IN</h4><p>Turn MEASURES AUTO FILL IN {C["E"]} to 16, 12, 8, 4 or 2: the fill plays automatically as the last bar of every
16, 12, 8, 4 or 2 bars. MANUAL turns it off.</p></div></div>'''), "BASIC COURSE", True)

    d.page(topic("Composing and Playing Rhythm Tracks", f'''<div class="cols2"><div>
<p>A Rhythm Track chains up to 64 rhythms into a song. There are 12 tracks: in COMPOSE and PLAY, INSTRUMENT-SELECT
{C["C"]} chooses the track (the orange RHYTHM TRACK numbers 1 &ndash; 12).</p><h4>COMPOSE</h4><ol>
<li>Set MODE {C["B"]} to COMPOSE and INSTRUMENT-SELECT {C["C"]} to the track number.</li>
<li>Press CLEAR {C["D"]} (TRACK CLEAR) to start an empty track.</li>
<li>Press a STEP key {C["R"]} to choose a rhythm, set BASIC-VARIATION {C["M"]}, and press TAP {C["T"]} to add it as the next bar.</li>
<li>Repeat 3 for each bar of the song.</li></ol></div>
<div><h4>PLAY</h4><ol><li>Set MODE {C["B"]} to PLAY and INSTRUMENT-SELECT {C["C"]} to the track.</li>
<li>Press Play in your DAW. The rhythms play in order and the track starts again at the end.
<span class="note">An empty track plays the rhythm chosen with the STEP keys, as in MANUAL PLAY.</span></li></ol></div></div>'''),
           "BASIC COURSE", True)

    d.page(topic("Control Descriptions", '<div class="cols3">' +
        ctl("A", "POWER Switch", "Opens the circuit board inside with its 29 trimmers (see INSIDE THE BOX). UNIT808 keeps playing; the black POWER switch or the power supply inside closes it again.") +
        ctl("B", "MODE Selector", "PATTERN CLEAR, 1st PART and 2nd PART (pattern write), MANUAL PLAY, PLAY and COMPOSE.") +
        ctl("C", "INSTRUMENT-SELECT", "The instrument the STEP keys write (AC, BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH). In PLAY and COMPOSE: the Rhythm Track 1 &ndash; 12. Its printed labels also mute and solo (see MUTE AND SOLO).") +
        ctl("D", "CLEAR Button", "PATTERN CLEAR mode: clears the rhythm. COMPOSE: clears the track (TRACK CLEAR). 1st/2nd PART: sets the step number (see ADVANCED COURSE).") +
        ctl("E", "MEASURES AUTO FILL IN", "MANUAL (off), 16, 12, 8, 4 or 2 bars.") +
        ctl("F", "SWING Control", "Delays every second step: 0 = straight, 10 = heavy shuffle (the off-beat at 75 % of the pair). Tempo always follows your DAW.") +
        ctl("G", "FLAM Control", "The time between the soft grace note and the main hit of flammed steps: TIGHT (5 ms) to LOOSE (40 ms).") +
        ctl("H", "LEVEL Controls", "The level of each sound channel, and ACCENT. Drag up or down; double-click returns a control to its default. The name window under each channel selects that instrument for writing.") +
        ctl("I", "TONE, TUNING, SNAPPY, DECAY", "Bass Drum tone and decay, Snare Drum tone and snappy, tom/conga tuning, Cymbal tone and decay, Open Hi-Hat decay.") +
        ctl("J", "Sound Selector Switches", "Low/Mid/Hi Tom or Conga, Rim Shot or Claves, Hand Clap or Maracas. Up = the upper label.") +
        ctl("K", "MASTER VOLUME", "The output level.") +
        ctl("L", "Transistor Labs Logo", "Opens the About window.") +
        ctl("M", "BASIC-VARIATION Lever", "A, AB (A then B) or B. When writing, A or B chooses the variation written.") +
        ctl("N", "Variation Indicators", "Show the playing variation.") +
        ctl("O", "START/STOP", "Shows your DAW&rsquo;s transport. In the standalone application it starts and stops; right-click it for the tempo.") +
        ctl("P", "PRE-SCALE Switch", "The step length: 1 = 16th-note triplets, 2 = 32nd notes, 3 = 16th notes (normal), 4 = 8th-note triplets.") +
        ctl("Q", "1st / 2nd PART Indicators", "The part being written or played.") +
        ctl("R", "STEP Keys", "Write steps (1st/2nd PART), choose rhythms 1 &ndash; 12 and fills 13 &ndash; 16 (MANUAL PLAY, PLAY, PATTERN CLEAR), choose the rhythm to add (COMPOSE).") +
        ctl("S", "I/F-VARIATION Lever", "The variation of the intro / fill.") +
        ctl("T", "TAP", "Fill in trigger (playing), tap-writing (1st/2nd PART), add the rhythm to the track (COMPOSE).") +
        '</div>'), "ADVANCED COURSE")

    d.page(topic("Step Number", f'''<p>Each part has 1 to 16 steps. In 1st or 2nd PART, press CLEAR {C["D"]} (its lamp lights),
then the STEP key of the last step. Shift-click a STEP key does the same. The steps after the last one stay dark.</p>''')
        + topic("2nd Part", f'''<p>Set MODE {C["B"]} to 2nd PART and write as in 1st PART. As soon as the 2nd Part has a step, it is
played after the 1st Part, giving a rhythm of up to 32 steps. Clear it (Alt-click CLEAR per instrument) and set its step number
as you like; a 2nd Part that was never written is not played.</p>''')
        + topic("Clearing", f'''<ul><li><b>A whole rhythm:</b> set MODE to PATTERN CLEAR, press the rhythm&rsquo;s STEP key, set BASIC-VARIATION
(A, B, or AB for both) and press CLEAR {C["D"]}.</li>
<li><b>One instrument of a part:</b> in 1st or 2nd PART select the instrument and Option/Alt-click CLEAR.</li>
<li><b>A track:</b> in COMPOSE press CLEAR (TRACK CLEAR).</li></ul><p class="note">Clearing has no undo; save your project first.</p>''')
        + topic("Copy and Paste", f'''<div class="cols2"><div>
<p><b>Whole rhythms and variations:</b> in MANUAL PLAY, PLAY, PATTERN CLEAR or COMPOSE, <b>right-click a STEP key</b> {C["R"]}
(rhythm 1 &ndash; 12 or intro/fill 13 &ndash; 16) for: Copy rhythm, Copy variation A, Copy variation B, Paste rhythm,
Paste to variation A / B, Undo paste. The key blinks after a paste.</p></div>
<div><p><b>Parts and instruments:</b> in 1st or 2nd PART, <b>right-click CLEAR</b> {C["D"]} (or a STEP key) for: Copy this part,
Copy the selected instrument, Paste into this part, Paste into the selected instrument, Undo paste.</p>
<p class="note">Copies include accents, flams and step numbers. Each UNIT808 has its own clipboard; it is not saved with the project.
Undo paste takes back the last paste.</p></div></div>'''),
        "ADVANCED COURSE", True)

    d.page(topic("Pre-Scale, Swing and Flam", f'''<div class="cols2"><p>PRE-SCALE {C["P"]} sets the length of a step:</p>
<table><tr><th>POSITION</th><th>STEP</th><th>16 STEPS =</th></tr>
<tr><td>1</td><td>16th-note triplet</td><td>2 &frac23; beats</td></tr><tr><td>2</td><td>32nd note</td><td>2 beats</td></tr>
<tr><td>3</td><td>16th note (normal)</td><td>one 4/4 bar</td></tr><tr><td>4</td><td>8th-note triplet</td><td>5 &frac13; beats</td></tr></table>
<p>SWING {C["F"]} delays every second step towards the next: at 0 the steps are even, at 10 the off-beat step comes 75 % of the way
through the pair. Rhythms stay lined up with the bars of your song.</p>
<h4>FLAM</h4><p>A flammed step plays a soft grace note on the step and the main hit just after it, like a drummer&rsquo;s flam.
Option/Alt-click a STEP key in 1st or 2nd PART to flam the selected instrument on that step (it lights blue). FLAM {C["G"]}
sets the gap, from TIGHT (5 ms) to LOOSE (40 ms).</p></div>''')
        + topic("Sound Controls", '''<table><tr><th>CHANNEL</th><th>CONTROLS</th><th>SWITCH</th></tr>
<tr><td>Bass Drum</td><td>Level, Tone, Decay</td><td></td></tr><tr><td>Snare Drum</td><td>Level, Tone, Snappy</td><td></td></tr>
<tr><td>Low / Mid / Hi</td><td>Level, Tuning</td><td>Tom or Conga</td></tr><tr><td>Rim Shot</td><td>Level</td><td>Rim Shot or Claves</td></tr>
<tr><td>Hand Clap</td><td>Level</td><td>Hand Clap or Maracas</td></tr><tr><td>Cow Bell</td><td>Level</td><td></td></tr>
<tr><td>Cymbal</td><td>Level, Tone, Decay</td><td></td></tr><tr><td>Open Hi-Hat</td><td>Level, Decay</td><td></td></tr>
<tr><td>Closed Hi-Hat</td><td>Level</td><td>(a closed hi-hat cuts off the open one)</td></tr></table>'''),
        "ADVANCED COURSE", True)

    d.page(topic("Mute and Solo", f"""<div class="cols2"><div>
<p>The printed labels around INSTRUMENT-SELECT {C["C"]} (AC, BD, SD &hellip; CH) mute and solo the instruments, also while the rhythm plays:</p>
<ul><li><b>Shift-click</b> a label: mute that instrument. The label is dimmed.</li>
<li><b>Option/Alt-click</b> a label: solo it. The label glows red. Several instruments can be soloed at once; then only they play.</li>
<li><b>Right-click</b> a label: a menu with Mute, Solo and Clear all mutes and solos.</li>
<li>A plain click selects the instrument, as before.</li></ul>
<p>The channel name windows (ACCENT, BASS DRUM &hellip;, LOW CONGA &hellip; MARACAS) work the same way: Shift-click mutes, Option/Alt-click solos,
right-click opens the menu; the dimmed or red state shows on the INSTRUMENT-SELECT labels.</p></div>
<div><p class="note">Muting AC plays the rhythm without accents. AC cannot be soloed.</p>
<p class="note">Mutes and solos also apply to MIDI notes, are saved with your project and can be automated.</p></div></div>""")
        + topic("Selecting with the Name Windows", f"""<p>Click a channel&rsquo;s name window (ACCENT, BASS DRUM, SNARE DRUM &hellip;, or LOW CONGA, CLAVES, MARACAS
above) to select that instrument for writing (Shift-click and Option/Alt-click mute and solo it), the same as turning INSTRUMENT-SELECT {C["C"]}. The selected channel&rsquo;s window
has an orange frame. Toms and congas share one row, so LOW TOM and LOW CONGA select the same instrument.</p>"""),
        "ADVANCED COURSE", True)

    d.page(topic("Playing from MIDI", '''<p>MIDI notes play the sounds (General MIDI drum map). Velocity works like accent:
with ACCENT at 0 every note plays at the same level, at 10 the level follows the velocity.</p><div class="cols3">
<table><tr><th>NOTE</th><th>SOUND</th></tr><tr><td>35, 36</td><td>Bass Drum</td></tr><tr><td>38, 40</td><td>Snare Drum</td></tr>
<tr><td>41, 43</td><td>Low Tom</td></tr><tr><td>45, 47</td><td>Mid Tom</td></tr><tr><td>48, 50</td><td>Hi Tom</td></tr></table>
<table><tr><th>NOTE</th><th>SOUND</th></tr><tr><td>37</td><td>Rim Shot</td></tr><tr><td>39</td><td>Hand Clap</td></tr>
<tr><td>56</td><td>Cow Bell</td></tr><tr><td>49, 51, 52, 55, 57, 59</td><td>Cymbal</td></tr><tr><td>42, 44 / 46</td><td>Closed / Open Hi-Hat</td></tr></table>
<table><tr><th>NOTE</th><th>SOUND</th></tr><tr><td>64</td><td>Low Conga</td></tr><tr><td>63</td><td>Mid Conga</td></tr>
<tr><td>62</td><td>Hi Conga</td></tr><tr><td>75</td><td>Claves</td></tr><tr><td>70</td><td>Maracas</td></tr></table></div>
<p class="note">Conga, claves and maracas notes play those sounds whatever the selector switches are set to.</p>''')
        + topic("Running in a DAW", '''<p>UNIT808 starts when your song plays and stops with it; START/STOP lights while it runs. It follows
the song&rsquo;s tempo, and when you jump or loop it lines the rhythm up with the bar again. The standalone application has
its own clock: press START/STOP, and right-click it to choose the tempo.</p>''')
        + topic("Saving and MIDI Mapping", '''<p>Everything is saved with your project: rhythms, parts, step numbers, tracks, knobs, switches and levers.
Every control is a plug-in parameter, so you can automate it and map it to a MIDI controller with your DAW&rsquo;s MIDI mapping
(Ableton Live: press Cmd/Ctrl-M, click a control on the plug-in&rsquo;s device, move the hardware control).</p>'''),
        "ADVANCED COURSE", True)

    d.page(topic("How Each Voice Is Modelled", '''<div class="cols2">
<p>Every sound is synthesized while you play: no samples. The engine is the one in the UNIT 808 Max for Live device,
built from the 808&rsquo;s circuit ideas.</p>
<ul>
<li><b>Bass Drum:</b> a resonating filter (the 808&rsquo;s bridged-T network) struck by the trigger pulse. Its pitch starts
high and drops in the first milliseconds for the punch, with a short click. TONE opens a low-pass filter, DECAY sets how long it rings.</li>
<li><b>Snare Drum:</b> two resonators (about 172 and 340 Hz) and high-passed noise. TONE sets the mix of the two resonators, SNAPPY the noise.</li>
<li><b>Toms:</b> a resonator whose pitch is higher while it is loud and slides down as it fades, with a little noise.
<b>Congas</b> are higher, shorter and without noise.</li>
<li><b>Rim Shot:</b> two resonators (455 and 1667 Hz) through a very short gate. <b>Claves:</b> one high resonator (2.4 kHz).</li>
<li><b>Hand Clap:</b> filtered noise with three fast bursts and a reverb-like tail. <b>Maracas:</b> high-passed noise with a short envelope.</li>
<li><b>Cow Bell:</b> two square-wave oscillators (540 and 800 Hz) through a band-pass filter with a fast-then-slow decay.</li>
<li><b>Cymbal, Open and Closed Hi-Hat:</b> six square-wave oscillators that run all the time (205, 304, 370, 523, 540 and
800 Hz), split into frequency bands. CY TONE moves the bands; the cymbal mixes a short bright band and longer lower ones.
A closed hi-hat cuts off the open one.</li>
<li><b>Accent:</b> accented steps play at full level; ACCENT sets how much softer the other steps are.</li></ul></div>'''),
        "ADVANCED COURSE", True)

    d.page(f'''<div class="topic" style="margin-bottom:6px"><div class="tt">Inside the Box</div><div class="tb">
<p>Flip the POWER switch {C["A"]} to open the circuit board inside. UNIT808 keeps playing, so you can tune the sounds while the rhythm runs.
Drag a trimmer up or down; a read-out shows its value; double-click returns it to its default. The 16 STEP keys on the lower board work just like the ones on the panel (writing, Shift / Alt, dragging, right-click menu) and their LEDs show the same state, with the 1st / 2nd PART lights beside them. To close the board, click the black POWER switch at top left or the power supply (the transformer on the left), or press Esc.
Trimmers are saved with your project and can be automated. At their defaults UNIT808 sounds exactly like the UNIT 808 device.</p></div></div>
<img class="board" src="{BOARD}">''', "ADVANCED COURSE", True)
    d.page(topic("The Trimmers", '''<div class="cols3">
<table><tr><th>BD</th><th>RANGE</th></tr><tr><td>TUNE</td><td>40 &ndash; 65 Hz (49.4)</td></tr><tr><td>CLICK</td><td>0 &ndash; 1 (0.01)</td></tr><tr><td>DRIVE</td><td>1 &ndash; 6 (1)</td></tr></table>
<table><tr><th>SD</th><th>RANGE</th></tr><tr><td>TUNE</td><td>0.8 &ndash; 1.25 &times; (1)</td></tr><tr><td>DECAY</td><td>0.5 &ndash; 2 &times; (0.73)</td></tr></table>
<table><tr><th>LT/LC &middot; MT/MC &middot; HT/HC</th><th>RANGE</th></tr><tr><td>TOM DECAY</td><td>20 &ndash; 400 ms (95 &middot; 70 &middot; 48)</td></tr>
<tr><td>TOM BEND</td><td>0 &ndash; 0.5 (0.03): pitch drop</td></tr><tr><td>CONGA DECAY</td><td>20 &ndash; 400 ms (86 &middot; 44 &middot; 38)</td></tr><tr><td>CONGA BEND</td><td>0 &ndash; 0.5 (0.03)</td></tr></table>
<table><tr><th>RS/CL</th><th>RANGE</th></tr><tr><td>RS TUNE, CL TUNE</td><td>0.7 &ndash; 1.4 &times; (1)</td></tr><tr><td>RS DECAY, CL DECAY</td><td>0.5 &ndash; 3 &times; (1)</td></tr></table>
<table><tr><th>CP</th><th>RANGE</th></tr><tr><td>TONE</td><td>600 &ndash; 5000 Hz (1300)</td></tr><tr><td>DECAY</td><td>10 &ndash; 300 ms (60): the reverb tail</td></tr></table>
<table><tr><th>CB</th><th>RANGE</th></tr><tr><td>TUNE</td><td>0.5 &ndash; 2 &times; (1)</td></tr><tr><td>DECAY</td><td>50 &ndash; 1200 ms (97)</td></tr></table>
<table><tr><th>CY</th><th>RANGE</th></tr><tr><td>TUNE</td><td>0.5 &ndash; 2 &times; (1)</td></tr></table>
<table><tr><th>HH (hi-hats)</th><th>RANGE</th></tr><tr><td>TUNE</td><td>0.5 &ndash; 2 &times; (1)</td></tr><tr><td>TONE</td><td>0.6 &ndash; 1.6 &times; (1)</td></tr><tr><td>CH DECAY</td><td>5 &ndash; 100 ms (24)</td></tr></table>
</div><p class="note">Defaults in brackets. The congas use the panel&rsquo;s TUNING and LEVEL controls of their channel; their decay and bend are here.</p>'''),
        "ADVANCED COURSE", True)

    d.page(topic("Exporting MIDI", f'''<p>UNIT808 turns its rhythms into standard MIDI clips you can drop into your DAW or save as .mid files.</p>
<ul><li><b>A rhythm or a variation:</b> right-click a STEP key in MANUAL PLAY, PLAY, PATTERN CLEAR or COMPOSE and choose
<i>Export rhythm</i> (variation A then B, with their 2nd parts), <i>Export variation A</i> or <i>Export variation B</i>.</li>
<li><b>A rhythm track (song):</b> in PLAY or COMPOSE the same menu offers <i>Export rhythm track</i> for the track INSTRUMENT-SELECT points at, bar by bar with its fills.</li>
<li><b>A part or one instrument:</b> in 1st or 2nd PART, right-click CLEAR and choose <i>Export this part</i> or <i>Export</i> the selected instrument.</li></ul>
<p>The export window then shows the clip. <b>Drag the clip</b> onto a track in your DAW, or click <b>SAVE .MID FILE</b>.
<b>WRITE SWING INTO THE NOTES</b> moves the notes the way SWING plays them (off: straight 16ths).
<b>LEAVE OUT MUTED / SOLOED-OUT</b> exports only what you hear. Esc or a click outside closes the window.</p>
<img src="{EXPORT}" style="display:block;width:58%;margin:6px auto">
<p class="note">Notes are General MIDI drums on channel 10, the same notes UNIT808 plays from: BD 36, SD 38, LT 41 (conga 64), MT 45 (63), HT 48 (62),
RS 37 (claves 75), CP 39 (maracas 70), CB 56, CY 49, OH 46, CH 42; the toms, RS and CP follow their selector switches. Accented steps have velocity 127,
the others 100; flams are a soft grace note and the main hit after the FLAM time. Tempo is the DAW&rsquo;s, 4/4, PRE-SCALE is respected.</p>'''), "ADVANCED COURSE", True)

    d.page(topic("About Window", '''<p>Click the Transistor Labs logo for the About window: version, build date, the format and host it runs in,
the contact address, the Transistor Labs store, and a button that opens this manual. Click anywhere to close it.</p>''')
        + topic("Window Size", '<p>Drag the bottom-right corner to resize the window, from 40 % to 100 % of the full panel size.</p>')
        + topic("Troubleshooting", '''<table><tr><th>PROBLEM</th><th>REMEDY</th></tr>
<tr><td>macOS won&rsquo;t open the installer</td><td>System Settings &rarr; Privacy &amp; Security &rarr; Open Anyway.</td></tr>
<tr><td>UNIT808 is not in Live&rsquo;s browser</td><td>Settings &rarr; Plug-Ins: turn on Audio Units v2 or VST3, then Rescan.</td></tr>
<tr><td>No sound</td><td>Check MASTER VOLUME; check that the song plays and the rhythm has steps; check that no instrument is muted (dimmed label) or another soloed (red label).</td></tr>
<tr><td>The STEP keys do not write</td><td>Writing works in 1st PART and 2nd PART. In the other modes the keys choose rhythms.</td></tr>
<tr><td>A rhythm change waits</td><td>While playing, a new rhythm starts at the next bar (its indicator blinks).</td></tr>
<tr><td>The wrong variation is written</td><td>Check the BASIC-VARIATION lever: A or B (AB writes A).</td></tr>
<tr><td>An old song can&rsquo;t find UNIT808</td><td>1.0 is a new Transistor Labs plug-in: songs saved with 0.4.x need UNIT808 1.0 loaded again.</td></tr>
<tr><td>A track does not play</td><td>MODE must be PLAY, and INSTRUMENT-SELECT must point at the track number.</td></tr></table>'''),
        "ADVANCED COURSE", True)

    d.page(topic("Specifications", f'''<div class="cols3">
<p><b>&#9744; UNIT808 &#9744; Rhythm Composer</b></p>
<p><b>&#9675; Memory</b><br>12 Basic Rhythms + 4 Intro / Fill In, each with variations A and B, each with a 1st and 2nd Part (1 &ndash; 16 steps each)</p>
<p><b>&#9675; Rhythm Tracks</b><br>12 tracks &times; 64 bars</p>
<p><b>&#9675; Sound Source</b><br>BD, SD, LT/LC, MT/MC, HT/HC, RS/CL, CP/MA, CB, CY, OH, CH, AC</p>
<p><b>&#9675; Controls</b><br>POWER, MODE, INSTRUMENT-SELECT, CLEAR, MEASURES AUTO FILL IN, SWING, FLAM, 22 sound controls,
5 sound switches, MASTER VOLUME, BASIC-VARIATION, I/F-VARIATION, PRE-SCALE, START/STOP, STEP &times; 16, TAP</p>
<p><b>&#9675; Trimmers</b><br>29 (inside the box)</p>
<p><b>&#9675; Indicators</b><br>Step &times; 16 (red; blue = flam), Variation A/B, 1st/2nd Part, Clear, Mute/Solo on the 12 instrument labels</p>
<p><b>&#9675; Timing</b><br>Follows the DAW&rsquo;s tempo and transport; Pre-Scale &times; 4; Swing</p>
<p><b>&#9675; Formats</b><br>macOS: AU, VST3, application<br>Windows: VST3, application<br>Linux: VST3, LV2, application</p>
<p><b>&#9675; Output</b><br>Stereo</p>
<p><b>&#9675; Version</b><br>{VERSION}, built with JUCE 8.0.9<br>Plug-in IDs: manufacturer Trlb, plug-in U808 (AU type aumu)</p>
<p><b>&#9675; Maker</b><br>Transistor Labs</p>
<p><b>&#9675; Contact</b><br>ticcoross@gmail.com<br>ticcoross.gumroad.com</p></div>''')
        + topic("System Requirements", '''<div class="cols3">
<p><b>&#9675; macOS</b><br>macOS 10.13 or later, Apple silicon or Intel (universal). AU or VST3 host, or the standalone application.</p>
<p><b>&#9675; Windows</b><br>Windows 10 or 11, 64-bit. VST3 host, or the standalone application.</p>
<p><b>&#9675; Linux</b><br>64-bit x86 with a VST3 or LV2 host, or the standalone application (JACK or ALSA).</p>
<p><b>&#9675; Computer</b><br>Any recent CPU: UNIT808 uses about 4% of one core at 48 kHz. 4 GB RAM. A screen of 1280 &times; 800 or larger.</p></div>'''), "ADVANCED COURSE", True)

    back(d, "Operation Manual")
    d.page(f'<img class="endbrand" src="{BRAND}">', cls="end")
    return d


# ================================================================ UPDATE HISTORY
HISTORY = [
    ("1.0.0", "", [
        "First release under the Transistor Labs name: new front panel logo, About window, manual and installers",
        "UNIT808 is now a new plug-in with its own Transistor Labs maker code. Songs saved with 0.4.x do not find it automatically: load UNIT808 1.0 again in those songs",
        "New vintage trimmers inside the box: only the screwdriver slot turns",
        "Mute and solo swapped: Shift-click a label mutes, Option/Alt-click solos",
    ]),
    ("0.4.3", "", [
        "Inside the box: the 16 STEP keys work like the panel ones, with live LEDs and the 1st / 2nd PART lights",
        "Inside the box: the black POWER switch, the power supply or Esc closes the board",
        "The Export MIDI window is now in the panel's own style, with 808-style keys and LEDs",
        "Right-click menus back to their 0.4.1 size",
        "Update history without dates",
    ]),
    ("0.4.2", "4 November 2026", [
        "MIDI export: rhythms, variations, parts, single instruments and whole rhythm tracks; drag the clip into your DAW or save a .mid file",
        "New inside: a photographed circuit board with the 29 trimmers on the voice board; click the Rhythm Composer lettering to close it",
        "Right-click menus fixed for real: they no longer shrink with the window size",
    ]),
    ("0.4.1", "4 November 2026", [
        "Right-click menus now really open at double size (larger text and rows)",
        "New circuit board artwork inside the box",
    ]),
    ("0.4.0", "4 November 2026", [
        "Inside the box: flip the POWER switch to open the circuit board with 29 trimmers: BD Tune, Click, Drive; SD Tune, Decay; Tom and Conga Decay and Bend for low, mid and hi; RS and CL Tune and Decay; CP Tone and Decay; CB Tune and Decay; CY Tune; HH Tune, Tone and CH Decay.",
        "The trimmers start at the UNIT 808 device's settings, so the sound is unchanged until you turn one. They are saved with your project, can be automated, and reset with a double-click. Click the Ticco Ross plate on the board to close it.",
        "POWER no longer switches UNIT808 off: the sound keeps playing while the board is open.",
        "Right-click menus are twice as big, in the panel's colours."]),
    ("0.3.0", "4 November 2026", [
        "Copy and paste rhythms: right-click a STEP key (in MANUAL PLAY, PLAY, PATTERN CLEAR or COMPOSE) to copy or paste a whole rhythm or one variation, A or B.",
        "Copy and paste parts and instruments: in 1st or 2nd PART, right-click CLEAR (or a STEP key) to copy or paste the part, or just the selected instrument.",
        "Copies include accents, flams and step numbers; Undo paste takes back the last paste. Each UNIT808 has its own clipboard.",
        "The channel name windows now also mute (Option/Alt-click) and solo (Shift-click) their instrument; right-click for the menu. A plain click still selects it.",
        "New Rhythm Composer lettering on the panel, closer to the original."]),
    ("0.2.0", "4 November 2026", [
        "FLAM: Option/Alt-click a STEP key in 1st or 2nd PART to play the selected instrument on that step as a flam (a soft grace note, then the main hit). Flams are per instrument; flammed steps light blue.",
        "The former FINE SWING knob is now FLAM: it sets the flam spacing from TIGHT (5 ms) to LOOSE (40 ms). SWING alone sets the swing.",
        "Mute and solo on the labels around INSTRUMENT-SELECT: Option/Alt-click to mute (the label dims), Shift-click to solo (it glows red), right-click for a menu. Muting AC removes the accents. All can be automated.",
        "The instrument name windows on the channels (ACCENT, BASS DRUM ... and LOW CONGA ... MARACAS) select the instrument for writing; the selected one has an orange frame.",
        "Changing MODE while the rhythm plays no longer restarts the bar or leaves a gap: the new mode carries on from the same step.",
        "Snare Drum level now starts at 3.0.",
        "No more help windows on the controls: only the small value read-outs while turning a knob. The mouse wheel no longer turns knobs and selectors; double-click still resets a knob."]),
    ("0.1.0", "4 November 2026", [
        "First release of the UNIT808 Rhythm Composer plug-in.",
        "The sound engine of the UNIT 808 Max for Live device, ported to the plug-in: Bass Drum, Snare Drum, Low/Mid/Hi Tom or Conga, Rim Shot or Claves, Hand Clap or Maracas (new), Cow Bell, Cymbal, Open and Closed Hi-Hat, with Accent.",
        "Hardware-style panel from the UNIT808 artwork with working knobs, selectors, switches, levers, keys and indicators. Resizable from 40 % to 100 %.",
        "808-style sequencer: 12 Basic Rhythms and 4 Intro/Fill In patterns, variations A and B, 1st and 2nd Part, step number, tap writing, Pattern Clear.",
        "BASIC-VARIATION A / AB / B, I/F-VARIATION, TAP fill in trigger and MEASURES AUTO FILL IN (2 - 16 bars).",
        "12 Rhythm Tracks of up to 64 bars (COMPOSE and PLAY, chosen with INSTRUMENT-SELECT).",
        "Always in time with your DAW: follows the tempo and transport; SWING and FINE SWING; PRE-SCALE 1 - 4.",
        "Plays from MIDI (General MIDI drum map, velocity as accent). Every control can be automated and MIDI-mapped; everything is saved with your project.",
        "About window with contact, store and the built-in manual. AU, VST3, LV2 and standalone application for macOS, Windows and Linux."]),
]


def history(extra=None):
    d = Doc()
    cover(d, "Update History")
    rows = (extra or []) + HISTORY
    chunks = [rows[i:i + 2] for i in range(0, len(rows), 2)]
    first = True
    for ch in chunks:
        b = ""
        for ver, date, items in ch:
            b += topic(f"Version {ver}",
                       '<ul class="cols2">' + "".join(f"<li>{i}</li>" for i in items) + "</ul>")
        d.page(b, "UPDATE HISTORY", not first)
        first = False
    back(d, "Update History")
    return d


if __name__ == "__main__":
    to_pdf(render(manual(), "UNIT808 Owner's Manual"), DOCS / "UNIT808 Manual.pdf")
    to_pdf(render(history(), "UNIT808 Update History"), DOCS / "UNIT808 Update History.pdf")
    print("done")
