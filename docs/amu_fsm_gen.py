#!/usr/bin/env python3
"""Generate docs/amu_fsm.drawio, the AMU parcel-run FSM state diagram.

Usage:  python3 docs/amu_fsm_gen.py [output.drawio]
        (default output: amu_fsm.drawio next to this script)

Every box is placed in absolute page coordinates on a 10 px grid; every edge
is an explicit orthogonal polyline that starts and ends on the border of its
source and target. Edge labels are placed beside a chosen segment. The layout
is therefore exact and reproducible, and a code change is a small diff here.

When Src/main.c or Src/Helper/helper.h changes, edit the matching State body,
edge or constant below, regenerate, then run 
to confirm the diagram still matches the code. Export the PNG from draw.io
(File > Export as > PNG, zoom 200%, border 20) to refresh amu_fsm_preview.png.

Optional: Pillow, used to measure label widths when placing labels. Without
it the widths are estimated.
"""
import html
import os
import sys

try:
    from PIL import ImageFont
except ImportError:  # label widths are then estimated
    ImageFont = None

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "amu_fsm.drawio")


def _load_font():
    if ImageFont is None:
        return None
    for path in ("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                 "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
                 "/Library/Fonts/Arial.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
                 "C:/Windows/Fonts/arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(path, 11)
        except OSError:
            continue
    return None


SANS = _load_font()


def text_width(s):
    if SANS is not None:
        return SANS.getlength(s)
    return 5.6 * len(s)  # Helvetica 11 px averages about 5.6 px per character


LINE_H = 13.5  # edge-label line height used for placement

BLACK = "#000000"
RED = "#b85450"
GREY_TXT = "#6b6b6b"

cells = []   # (z, xml)
reg = {}     # id -> (x, y, w, h, parent) absolute


def h(s):
    return html.escape(s, quote=False)


def attr(s):
    return html.escape(s, quote=True)


def box(cid, parent, x, y, w, hgt):
    reg[cid] = (x, y, w, hgt, parent)


def rel(cid):
    x, y, w, hgt, parent = reg[cid]
    if parent in reg:
        px, py = reg[parent][0], reg[parent][1]
        return x - px, y - py, w, hgt
    return x, y, w, hgt


def add(z, xml):
    cells.append((z, xml))


# ---------------------------------------------------------------- text
def body_html(lines):
    out = []
    for line in lines:
        indent = len(line) - len(line.lstrip(" "))
        s = line.lstrip(" ")
        pre = "&nbsp;" * indent
        if s.startswith("//"):
            txt = '<font color="%s"><i>%s</i></font>' % (GREY_TXT, h(s).replace("  ", "&nbsp;&nbsp;"))
        else:
            txt = h(s).replace("  ", "&nbsp;&nbsp;")
            for kw in ("entry /", "do /", "exit /"):
                if s.startswith(kw):
                    txt = "<b>%s</b>%s" % (kw, h(s[len(kw):]).replace("  ", "&nbsp;&nbsp;"))
                    break
        out.append(pre + txt)
    return '<div style="line-height:14px">' + "<br>".join(out) + "</div>"


def text_size(lines):
    w = max(text_width(l) for l in lines)
    return w, LINE_H * len(lines)


# ------------------------------------------------------------- shapes
SWIM = ("swimlane;fontStyle=1;align=center;verticalAlign=middle;startSize=%d;horizontal=1;html=1;"
        "whiteSpace=wrap;collapsible=0;fontSize=%d;fontFamily=Helvetica;rounded=1;absoluteArcSize=1;"
        "arcSize=12;fillColor=%s;strokeColor=%s;strokeWidth=1.5;swimlaneFillColor=%s;%s")


def state(cid, parent, x, y, w, name, body, kind="state", hgt=None):
    nlines = len(body)
    if hgt is None:
        hgt = 26 + 6 + 14 * nlines + 6
    box(cid, parent, x, y, w, hgt)
    if kind == "fault":
        fill, stroke, extra = "#f8cecc", RED, ""
    elif kind == "stub":
        fill, stroke, extra = "#ffffff", BLACK, "dashed=1;dashPattern=5 3;"
    else:
        fill, stroke, extra = "#ffffff", BLACK, ""
    rx, ry, rw, rh = rel(cid)
    par = parent if parent in reg else "1"
    add(1, '<mxCell id="%s" value="%s" style="%s" vertex="1" parent="%s">'
           '<mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(name), SWIM % (26, 13, fill, stroke, fill, extra), par, rx, ry, rw, rh))
    add(2, '<mxCell id="%s_body" value="%s" style="text;html=1;align=left;verticalAlign=top;spacingLeft=8;'
           'spacingRight=4;spacingTop=4;spacingBottom=0;fontSize=11;fontFamily=Courier New;overflow=hidden;'
           'whiteSpace=nowrap;" vertex="1" parent="%s"><mxGeometry x="0" y="26" width="%g" height="%g" '
           'as="geometry"/></mxCell>' % (cid, attr(body_html(body)), cid, rw, rh - 26))
    return hgt


def composite(cid, x, y, w, hgt, name):
    box(cid, "1", x, y, w, hgt)
    add(0, '<mxCell id="%s" value="%s" style="%s" vertex="1" parent="1">'
           '<mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(name), SWIM % (28, 14, "#eeeeee", "#666666", "#f7f7f7", ""), x, y, w, hgt))


def note(cid, parent, x, y, w, hgt, lines):
    box(cid, parent, x, y, w, hgt)
    rx, ry, rw, rh = rel(cid)
    par = parent if parent in reg else "1"
    val = "<br>".join(h(l) for l in lines)
    add(1, '<mxCell id="%s" value="%s" style="shape=note;size=10;whiteSpace=wrap;html=1;backgroundOutline=1;'
           'fillColor=#ffffff;strokeColor=#808080;strokeWidth=1;align=left;verticalAlign=top;spacingLeft=6;'
           'spacingRight=6;spacingTop=3;fontSize=10.5;fontFamily=Helvetica;fontColor=#444444;" vertex="1" '
           'parent="%s"><mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(val), par, rx, ry, rw, rh))


def plain(cid, x, y, w, hgt, value, size=11, color="#333333", bold=False):
    box(cid, "1", x, y, w, hgt)
    add(1, '<mxCell id="%s" value="%s" style="text;html=1;align=left;verticalAlign=top;spacing=0;'
           'fontSize=%g;fontFamily=Helvetica;fontColor=%s;whiteSpace=wrap;%s" vertex="1" parent="1">'
           '<mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(value), size, color, "fontStyle=1;" if bold else "", x, y, w, hgt))


# -------------------------------------------------------------- edges
def polyline_mid(pts):
    segs = []
    total = 0.0
    for a, b in zip(pts, pts[1:]):
        d = abs(b[0] - a[0]) + abs(b[1] - a[1])
        segs.append(d)
        total += d
    half = total / 2
    acc = 0.0
    for (a, b), d in zip(zip(pts, pts[1:]), segs):
        if acc + d >= half:
            t = (half - acc) / d if d else 0
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        acc += d
    return pts[-1]


def frac(cid, p):
    x, y, w, hgt, _ = reg[cid]
    return (p[0] - x) / w, (p[1] - y) / hgt


def edge(eid, src, tgt, pts, color=BLACK, lines=None, seg=0, side="above", gap=4, shift=0, sw=1.5):
    (sx, sy), (tx, ty) = pts[0], pts[-1]
    ex, ey = frac(src, pts[0])
    nx, ny = frac(tgt, pts[-1])
    for a, b in zip(pts, pts[1:]):
        assert a[0] == b[0] or a[1] == b[1], "non-orthogonal segment in %s: %s -> %s" % (eid, a, b)
    style = ("edgeStyle=none;rounded=0;html=1;endArrow=open;endFill=0;endSize=10;strokeColor=%s;strokeWidth=%g;"
             "fontSize=11;fontFamily=Helvetica;fontColor=%s;labelBackgroundColor=none;align=center;"
             "exitX=%.4f;exitY=%.4f;exitDx=0;exitDy=0;exitPerimeter=0;"
             "entryX=%.4f;entryY=%.4f;entryDx=0;entryDy=0;entryPerimeter=0;"
             % (color, sw, color, ex, ey, nx, ny))
    value, off = "", (0, 0)
    if lines:
        w, hh = text_size(lines)
        a, b = pts[seg], pts[seg + 1]
        if a[1] == b[1]:  # horizontal segment
            cx = (a[0] + b[0]) / 2 + shift
            cy = a[1] - gap - hh / 2 if side == "above" else a[1] + gap + hh / 2
        else:             # vertical segment
            cy = (a[1] + b[1]) / 2 + shift
            cx = a[0] + gap + w / 2 if side == "right" else a[0] - gap - w / 2
        mx, my = polyline_mid(pts)
        off = (cx - mx, cy - my)
        value = "<br>".join(h(l) for l in lines)
    wp = ""
    inner = pts[1:-1]
    if inner:
        wp = '<Array as="points">' + "".join('<mxPoint x="%g" y="%g"/>' % p for p in inner) + "</Array>"
    add(3, '<mxCell id="%s" value="%s" style="%s" edge="1" parent="1" source="%s" target="%s">'
           '<mxGeometry relative="1" as="geometry"><mxPoint x="%.1f" y="%.1f" as="offset"/>%s</mxGeometry></mxCell>'
        % (eid, attr(value), style, src, tgt, off[0], off[1], wp))


# =================================================================== LAYOUT
PAGE_W, PAGE_H = 1960, 1740

plain("title", 40, 20, 900, 30, "<b>AMU parcel-run FSM</b>", size=20, color="#000000")
plain("subtitle", 40, 52, 1800, 18,
      "One box per State in the Src/main.c switch (the ST_* enum). UML state-machine notation: "
      "trigger [guard] / effect. Terms follow GLOSSARY.md. A / B is the Layout learned by IDENTIFY.",
      size=11, color="#555555")

# ---- composites (drawn first so states sit on top)
composite("search_phase", 440, 130, 800, 200, "SEARCH (phase)")
composite("storage", 40, 620, 500, 660, "STORAGE")
composite("pathfind", 600, 620, 1310, 660, "PATHFIND")

# ---- initial pseudo-state and IDLE
box("start", "1", 152, 100, 16, 16)
add(1, '<mxCell id="start" value="" style="ellipse;html=1;fillColor=#000000;strokeColor=#000000;" vertex="1" '
       'parent="1"><mxGeometry x="152" y="100" width="16" height="16" as="geometry"/></mxCell>')

state("idle", "1", 40, 168, 240, "IDLE", [
    "do / Kobuki_Drive(STOP)",
    "exit / layout = none",
    "       object.detected = 0",
    "       heading = 0",
    "       corridor = 0",
    "       Scan_Start(kobuki.angle)"], hgt=134)

# ---- SEARCH phase
state("search", "search_phase", 470, 200, 260, "SEARCH", [
    "do / Scan_Sample()",
    "     Kobuki_Rotate(150)  // ACW"], hgt=70)
state("identify", "search_phase", 900, 200, 300, "IDENTIFY", [
    "entry / Kobuki_Drive(STOP)"], hgt=70)

# ---- ORIENT
state("orient", "1", 1390, 180, 300, "ORIENT", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: -75, B: +108)",
    "         // A: CW, B: ACW",
    "do / Kobuki_Rotate_Angle(kobuki.angle)"], hgt=110)

# ---- FAULT bar
state("fault", "1", 440, 440, 1030, "STOP / FAULT", [
    "entry / Kobuki_Drive(STOP)"], kind="fault", hgt=54)

# ---- PATHFIND
state("p_drive", "pathfind", 1380, 700, 320, "DRIVE", [
    "do / Kobuki_Drive(150)",
    "     // at the speed limit"], hgt=80)

box("p_choice", "1", 1096, 706, 28, 28)
add(1, '<mxCell id="p_choice" value="" style="rhombus;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1.5;" '
       'vertex="1" parent="1"><mxGeometry x="1096" y="706" width="28" height="28" as="geometry"/></mxCell>')

state("p_creverse", "pathfind", 950, 840, 320, "CORRIDOR_REVERSE", [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, -50)",
    "do / Kobuki_Drive_Distance(kobuki.distance)",
    "// bumper and cliff are ignored here"], hgt=94)
state("p_cturn", "pathfind", 950, 1010, 320, "CORRIDOR_TURN", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: +90, B: -90)",
    "         // A: ACW, B: CW",
    "do / Kobuki_Rotate_Angle(kobuki.angle)",
    "exit / heading = kobuki.angle",
    "// bumper and cliff are ignored here"], hgt=124)
state("p_cstop", "pathfind", 650, 1094, 240, "CORRIDOR_STOP", [
    "entry / Kobuki_Drive(STOP)"], hgt=54)

state("p_reverse", "pathfind", 1380, 930, 320, "REVERSE", [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, -50)",
    "do / Kobuki_Drive_Distance(kobuki.distance)",
    "// bumper and cliff are ignored here"], hgt=94)
state("p_rotate", "pathfind", 1380, 1090, 320, "ROTATE", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, pathfind_recover_deg)",
    "         // latched in DRIVE: wall A -5, B +5;",
    "         // ledge A +5, B -5",
    "do / Kobuki_Rotate_Angle(kobuki.angle)",
    "// bumper and cliff are ignored here"], hgt=124)

note("pathfind_note", "pathfind", 640, 1196, 660, 58, [
    "Runs twice: corridor = 1 ends in CORRIDOR_REVERSE, then CORRIDOR_TURN; corridor = 2 ends in "
    "CORRIDOR_STOP. A corridor ends on the front cliff, not on distance. DRIVE ignores the ledge-side "
    "bumper and the storage-wall-side cliff; REVERSE, ROTATE and the CORRIDOR_ States ignore bumper and "
    "cliff. Only B1 | wheeldrop faults here."])

# ---- STORAGE
state("deposit", "storage", 90, 690, 400, "DEPOSIT (stub)", [
    "entry / Kobuki_Drive(STOP)",
    "// stub: deposit-location sensing",
    "//   is not built yet"], kind="stub", hgt=80)
state("drive_in", "storage", 90, 870, 400, "DRIVE_IN", [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, 900)",
    "         // through the Entrance",
    "do / Kobuki_Drive_Distance(kobuki.distance)"], hgt=94)
state("turn", "storage", 90, 1074, 400, "TURN", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: +131, B: -131)",
    "         // A: ACW, B: CW (face the Entrance)",
    "do / Kobuki_Rotate_Angle(kobuki.angle)"], hgt=94)
note("storage_note", "storage", 70, 1200, 460, 46, [
    "TURN ignores bumper and cliff (the turn is made beside the ledge). DRIVE_IN ignores the cliff; "
    "any bumper ends it in DEPOSIT. Only B1 | wheeldrop faults here."])

# ===================================================================== EDGES
# main flow: start -> IDLE -> SEARCH -> IDENTIFY -> ORIENT -> DRIVE -> ...
edge("e_start", "start", "idle", [(160, 116), (160, 168)])
edge("e_idle_search", "idle", "search", [(280, 235), (470, 235)],
     lines=["B0", "[kobuki.button & 0x01]"], seg=0, side="above", shift=-20)
edge("e_search_identify", "search", "identify", [(730, 235), (900, 235)],
     lines=["1: [object.detected != 0]"], seg=0, side="above")
edge("e_identify_orient", "identify", "orient", [(1200, 235), (1390, 235)],
     lines=["ε", "/ layout = A | B", "LED1 red | yellow"], seg=0, side="above", shift=20)
edge("e_orient_drive", "orient", "p_drive", [(1540, 290), (1540, 700)],
     lines=["kobuki.rotation_complete", "/ heading = kobuki.angle,", "corridor = 1"], seg=0, side="right",
     gap=8, shift=-60)

# fault edges (red)
edge("f_search_phase", "search_phase", "fault", [(1000, 330), (1000, 440)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop", "| bumper | cliff"], seg=0, side="right", gap=8)
edge("f_search_giveup", "search", "fault", [(600, 270), (600, 440)], color=RED, sw=1.25,
     lines=["2: [scan_deg >= 360]", "/ LED1 green"], seg=0, side="right", gap=8, shift=30)
edge("f_orient", "orient", "fault", [(1420, 290), (1420, 440)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop", "| bumper | cliff"], seg=0, side="left", gap=8, shift=20)
edge("f_pathfind", "pathfind", "fault", [(1300, 620), (1300, 494)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop"], seg=0, side="left", gap=8)
edge("f_storage", "storage", "fault", [(500, 620), (500, 494)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop"], seg=0, side="left", gap=8)

# returns to IDLE
edge("e_fault_idle", "fault", "idle", [(440, 467), (240, 467), (240, 302)],
     lines=["ε"], seg=0, side="above", gap=4)
edge("e_deposit_idle", "deposit", "idle", [(120, 690), (120, 302)],
     lines=["ε", "(stub: at once)"], seg=0, side="right", gap=8, shift=50)

# PATHFIND
edge("p_drive_choice", "p_drive", "p_choice", [(1380, 720), (1124, 720)],
     lines=["1: front cliff", "[kobuki.cliff & 0x02]"], seg=0, side="above")
edge("p_choice_creverse", "p_choice", "p_creverse", [(1110, 734), (1110, 840)],
     lines=["[corridor == 1]"], seg=0, side="right", gap=8)
edge("p_choice_stop", "p_choice", "p_cstop", [(1096, 720), (770, 720), (770, 1094)],
     lines=["[corridor == 2]"], seg=0, side="above")
edge("p_creverse_cturn", "p_creverse", "p_cturn", [(1110, 934), (1110, 1010)],
     lines=["kobuki.distance_complete"], seg=0, side="right", gap=8)
edge("p_cturn_drive", "p_cturn", "p_drive", [(1270, 1072), (1320, 1072), (1320, 760), (1380, 760)],
     lines=["kobuki.rotation_complete", "/ corridor = 2"], seg=1, side="right", gap=8, shift=-120)
edge("p_drive_reverse", "p_drive", "p_reverse", [(1600, 780), (1600, 930)],
     lines=["2: storage-wall bumper", "[A: kobuki.bumper & 0x04,", " B: kobuki.bumper & 0x01]",
            "/ recover = A -5 | B +5", " ", "3: ledge cliff", "[A: kobuki.cliff & 0x01,",
            " B: kobuki.cliff & 0x04]", "/ recover = A +5 | B -5"], seg=0, side="right", gap=10)
edge("p_reverse_rotate", "p_reverse", "p_rotate", [(1540, 1024), (1540, 1090)],
     lines=["kobuki.distance_complete"], seg=0, side="right", gap=8)
edge("p_rotate_drive", "p_rotate", "p_drive", [(1700, 1152), (1850, 1152), (1850, 740), (1700, 740)],
     lines=["kobuki.", "rotation_complete"], seg=1, side="left", gap=8, shift=100)
edge("p_stop_turn", "p_cstop", "turn", [(650, 1121), (490, 1121)],
     lines=["ε"], seg=0, side="above")

# STORAGE
edge("e_turn_drivein", "turn", "drive_in", [(290, 1074), (290, 964)],
     lines=["kobuki.rotation_complete"], seg=0, side="right", gap=8)
edge("e_drivein_deposit_bump", "drive_in", "deposit", [(140, 870), (140, 770)],
     lines=["1: any bumper", "[kobuki.bumper != 0]", "/ Kobuki_Drive(STOP)"], seg=0, side="right", gap=8)
edge("e_drivein_deposit_done", "drive_in", "deposit", [(440, 870), (440, 770)],
     lines=["2: kobuki.", "distance_complete"], seg=0, side="left", gap=8)

# ===================================================================== LEGEND
LY = 1330
box("lg_dot", "1", 44, LY + 6, 16, 16)
add(1, '<mxCell id="lg_dot" value="" style="ellipse;html=1;fillColor=#000000;strokeColor=#000000;" vertex="1" '
       'parent="1"><mxGeometry x="44" y="%d" width="16" height="16" as="geometry"/></mxCell>' % (LY + 6))
plain("lg_dot_t", 70, LY + 6, 150, 18, "start (initial State)")
box("lg_dia", "1", 230, LY, 28, 28)
add(1, '<mxCell id="lg_dia" value="" style="rhombus;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1.5;" '
       'vertex="1" parent="1"><mxGeometry x="230" y="%d" width="28" height="28" as="geometry"/></mxCell>' % LY)
plain("lg_dia_t", 266, LY + 6, 350, 18, "choice: one trigger, the guard on each edge picks the way")

for i, (cid, label, fill, stroke, dash) in enumerate([
        ("lg_st", "state", "#ffffff", BLACK, ""),
        ("lg_fa", "fault", "#f8cecc", RED, ""),
        ("lg_su", "composite (phase)", "#eeeeee", "#666666", ""),
        ("lg_stub", "stub", "#ffffff", BLACK, "dashed=1;dashPattern=5 3;")]):
    x = [660, 770, 880, 1080][i]
    w = [90, 90, 170, 90][i]
    add(1, '<mxCell id="%s" value="%s" style="rounded=1;absoluteArcSize=1;arcSize=12;html=1;fontSize=11;'
           'fontFamily=Helvetica;fillColor=%s;strokeColor=%s;strokeWidth=1.5;%s" vertex="1" parent="1">'
           '<mxGeometry x="%d" y="%d" width="%d" height="30" as="geometry"/></mxCell>'
        % (cid, attr(label), fill, stroke, dash, x, LY - 1, w))
plain("lg_red", 1190, LY + 6, 700, 18,
      '<font color="%s">red edge</font> = fault: the State leaves for STOP / FAULT' % RED)

# ===================================================================== NOTES
NY = 1400
NW = 440
cols = [40, 530, 1020, 1510]

n1 = ("<b>How to read it</b><br>A State is a rounded box: its name, then its actions. <b>entry /</b> runs once on "
      "entering, <b>do /</b> on every Tick in which no edge fired, <b>exit /</b> once on leaving. Edges read: "
      "trigger [guard] / effect. A State's edges are checked in priority order, the red fault edge first, and "
      "the first true one fires; a transition runs exit(), then the effect, then entry() of the next State. "
      "1:, 2:, 3: number the edges that share a source.<br><br>"
      "<b>ε</b> = no trigger: the edge is taken on the next Tick. One Tick = one Kobuki packet: Kobuki_Read() "
      "runs first, then the state switch.<br><br>"
      "Angles are degrees: + = ACW (left), - = CW (right). <b>heading</b> = kobuki.angle, latched when ORIENT "
      "completes and when CORRIDOR_TURN is left.<br><br>"
      "Setpoints (Kobuki_Rotate_Angle_Setpoint, Kobuki_Drive_Distance_Setpoint) are one-shot entry actions. "
      "kobuki.rotation_complete and kobuki.distance_complete are copied at the top of the Tick by "
      "Kobuki_Read(), so a State sees them one Tick after its last do / call.")

n2 = ("<b>Layouts</b><br>A / B is the Layout saved by IDENTIFY, never a live reading. Layout A: bollards A1/A2 "
      "block the left/top route; ORIENT turns clockwise to the bottom corridor; the storage wall is on the "
      "AMU's left and the ledge on its right. Layout B is the mirror image: bollards B1/B2 block the "
      "bottom/right route; ORIENT turns anticlockwise to the left corridor.<br><br>"
      "<b>Sensors</b><br>Bumper / cliff bits: right = 0x01, centre = 0x02, left = 0x04. B0 = kobuki.button "
      "&amp; 0x01 (start). B1 = kobuki.button &amp; 0x02 (stop). Fault_B1_Wheeldrop() = B1 | "
      "kobuki.wheeldrop != 0. Fault_Any() = that | any bumper | any cliff.<br><br>"
      "<b>Faults</b><br>B1 | wheeldrop leaves every State except IDLE and STOP / FAULT. SEARCH, IDENTIFY and "
      "ORIENT also fault on any bumper or cliff. Bumpers and cliffs are expected in PATHFIND and STORAGE, so "
      "those regions have one red edge each, for B1 | wheeldrop only. The only timeout is the SEARCH "
      "give-up. STOP / FAULT stops the motors and returns to IDLE on the next Tick.")

n3 = ("<b>Search phase</b><br>SEARCH turns anticlockwise on the spot from B0 and reads the ultrasonic once per "
      "Tick. <b>scan_deg</b> is the net turn since B0 in degrees, ACW positive, summed from gyro changes so "
      "it does not wrap at 180; it only drives the give-up (scan_deg >= 360: LED1 green, then FAULT). A "
      "Detection is range_mm &lt; 750. IDENTIFY stops on the first Detection, then layout = "
      "Layout_From_Range(range_mm): B when range_mm &lt; 575, otherwise A. LED1 red for A, yellow for B. "
      "There is no centring: ORIENT turns from wherever SEARCH stopped.<br><br>"
      "<b>IDLE reset</b><br>Leaving IDLE (B0) clears layout, object.detected, heading and corridor, and "
      "zeroes the scan with Scan_Start().<br><br>"
      "<b>Calibration (layout split, detect range)</b><br>The split is a calibration value (575 mm now). "
      "IDENTIFY reads the first in-range reading, taken as the sonar beam first reaches the bollard, so "
      "calibrate the same way: turn the AMU anticlockwise onto B1, then onto A1, and read "
      "robot.object.range_mm when SEARCH stops; the split goes between them. The detect range must stay "
      "above the A1 reading and below the nearest wall.")

n4 = ("<b>Constants (Src/Helper/helper.h)</b><br>Scan: turn speed 150, give-up after a full turn of 360. "
      "Layout split 575 mm (B below, A otherwise). Orient A -75 / B +108. Pathfind: drive speed 150, reverse "
      "50 mm, wall-hit nudge A -5 / B +5 (ledge cliff: the opposite), corridor turn A +90 / B -90. Storage: "
      "turn A +131 / B -131, drive-in 900 mm. Detect range 750 mm (Src/Ultrasonic/ultrasonic.h). The Kobuki "
      "speed limit is 150 mm/s.<br><br>"
      "<b>Helpers (Src/Helper/helper.c)</b><br>Layout_From_Range(range_mm) is the single split; "
      "Layout_Pick(a, b) picks a value by Layout. Scan_Sample() = Scan_Update_Turn(kobuki.angle) + "
      "Ultrasonic_Detect(&amp;object); Scan_Gave_Up() = scan_deg >= 360. Corridor_Take_Wall_Hit() and "
      "Corridor_Take_Ledge_Hit() test the Layout's side and latch pathfind_recover_deg.")

for i, (t) in enumerate([n1, n2, n3, n4]):
    # the notes are already HTML; store them as-is
    box("note_col%d" % (i + 1), "1", cols[i], NY, NW, 330)
    add(1, '<mxCell id="note_col%d" value="%s" style="text;html=1;align=left;verticalAlign=top;spacing=0;'
           'fontSize=11;fontFamily=Helvetica;fontColor=#333333;whiteSpace=wrap;" vertex="1" parent="1">'
           '<mxGeometry x="%d" y="%d" width="%d" height="330" as="geometry"/></mxCell>'
        % (i + 1, attr(t), cols[i], NY, NW))

# ===================================================================== OUTPUT
cells.sort(key=lambda c: c[0])  # stable: composites, states, bodies/notes, edges
xml = ('<mxfile host="app.diagrams.net" agent="amu-fsm-generator" version="24.7.5"><diagram id="amu_fsm" '
       'name="AMU FSM"><mxGraphModel dx="1400" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" '
       'connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="%d" pageHeight="%d" math="0" '
       'shadow="0"><root><mxCell id="0"/><mxCell id="1" parent="0"/>' % (PAGE_W, PAGE_H))
xml += "".join(c[1] for c in cells)
xml += "</root></mxGraphModel></diagram></mxfile>"
open(OUT, "w").write(xml)
print("wrote", OUT, len(cells), "cells")
