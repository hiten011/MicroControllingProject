#!/usr/bin/env python3
"""Generate docs/amu_fsm.drawio, the AMU parcel-run FSM state diagram.

Usage:  python3 docs/amu_fsm_gen.py [output.drawio]
        (default output: amu_fsm.drawio next to this script)

Every box is placed in absolute page coordinates on a grid; every edge is an explicit orthogonal polyline
that starts and ends on the border of its source and target. Edge labels are placed beside a chosen segment.
The layout is therefore exact and reproducible, and a code change is a small diff here.

When Src/main.c or Src/Helper/helper.h changes, edit the matching State body, edge or constant below,
regenerate, then run docs/amu_fsm_check.py to confirm the diagram still matches the code. Then run
docs/amu_fsm_render.js to refresh amu_fsm_preview.png (or export it from draw.io: File > Export as > PNG,
zoom 200%, border 20).

The generator also lints its own layout (box overlaps, labels on boxes or lines, edges through boxes, edge
crossings) and prints every problem it finds; a clean run prints "layout lint: 0 problems".

Layout conventions (UML state-machine style, after Ambler's "Elements of UML 2.0 Style"):
  * the initial pseudostate is top-left; the flow reads clockwise (SEARCH left to right, PATHFIND right to
    left, STORAGE bottom to top, back to IDLE);
  * a transition reads  trigger [guard] / effect;  sibling guards are mutually exclusive;
  * a label sits beside its edge, near the source: above a left-to-right line, below a right-to-left line,
    left of an upward line and right of a downward one (the side with free space wins when the rule is blocked);
  * common transitions are drawn once on the composite that holds the States (one red fault edge per region).

Optional: Pillow, used to measure label widths when placing labels. Without it the widths are estimated.
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
obst = {}    # id -> (x, y, w, h) boxes that lines and labels must keep clear of
edges = []   # lint records: (eid, src, tgt, pts, label_rect)


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
    obst[cid] = (x, y, w, hgt)
    if kind == "fault":
        fill, stroke = "#f8cecc", RED
    else:
        fill, stroke = "#ffffff", BLACK
    rx, ry, rw, rh = rel(cid)
    par = parent if parent in reg else "1"
    add(1, '<mxCell id="%s" value="%s" style="%s" vertex="1" parent="%s">'
           '<mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(name), SWIM % (26, 13, fill, stroke, fill, ""), par, rx, ry, rw, rh))
    add(2, '<mxCell id="%s_body" value="%s" style="text;html=1;align=left;verticalAlign=top;spacingLeft=8;'
           'spacingRight=4;spacingTop=4;spacingBottom=0;fontSize=11;fontFamily=Courier New;overflow=hidden;'
           'whiteSpace=nowrap;" vertex="1" parent="%s"><mxGeometry x="0" y="26" width="%g" height="%g" '
           'as="geometry"/></mxCell>' % (cid, attr(body_html(body)), cid, rw, rh - 26))
    return hgt


def composite(cid, x, y, w, hgt, name, parent="1", nested=False):
    box(cid, parent, x, y, w, hgt)
    rx, ry, rw, rh = rel(cid)
    fill, stroke, swim_fill = ("#e4e4e4", "#666666", "#efefef") if nested else ("#eeeeee", "#666666", "#f7f7f7")
    add(0 if not nested else 0.5, '<mxCell id="%s" value="%s" style="%s" vertex="1" parent="%s">'
        '<mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, attr(name), SWIM % (28 if not nested else 24, 14 if not nested else 12, fill, stroke, swim_fill, ""),
           parent, rx, ry, rw, rh))


def note(cid, parent, x, y, w, hgt, lines):
    box(cid, parent, x, y, w, hgt)
    obst[cid] = (x, y, w, hgt)
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


def pseudo(cid, shape, x, y, size, fill):
    box(cid, "1", x, y, size, size)
    if shape == "rhombus":
        obst[cid] = (x, y, size, size)
    add(1, '<mxCell id="%s" value="" style="%s;html=1;fillColor=%s;strokeColor=#000000;strokeWidth=1.5;" '
           'vertex="1" parent="1"><mxGeometry x="%g" y="%g" width="%g" height="%g" as="geometry"/></mxCell>'
        % (cid, shape, fill, x, y, size, size))


# ---------------------------------------------------- geometry helpers
def gx(cid):
    return reg[cid][0]


def gy(cid):
    return reg[cid][1]


def right(cid):
    return reg[cid][0] + reg[cid][2]


def bottom(cid):
    return reg[cid][1] + reg[cid][3]


def cx(cid):
    return reg[cid][0] + reg[cid][2] / 2


def cy(cid):
    return reg[cid][1] + reg[cid][3] / 2


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


def lbl_right_of(x, y, lines, gap=8):
    """Label centre for a label whose left edge sits `gap` px right of x, vertically centred on y."""
    return (x + gap + text_size(lines)[0] / 2, y)


def lbl_left_of(x, y, lines, gap=8):
    """Label centre for a label whose right edge sits `gap` px left of x, vertically centred on y."""
    return (x - gap - text_size(lines)[0] / 2, y)


def edge(eid, src, tgt, pts, color=BLACK, lines=None, seg=0, side="above", gap=4, shift=0, sw=1.5, at=None):
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
    value, off, lab = "", (0, 0), None
    if lines:
        w, hh = text_size(lines)
        a, b = pts[seg], pts[seg + 1]
        if at is not None:                       # explicit label centre
            lcx, lcy = at
        elif a[1] == b[1]:                       # horizontal segment
            lcx = (a[0] + b[0]) / 2 + shift
            lcy = a[1] - gap - hh / 2 if side == "above" else a[1] + gap + hh / 2
        else:                                    # vertical segment
            lcy = (a[1] + b[1]) / 2 + shift
            lcx = a[0] + gap + w / 2 if side == "right" else a[0] - gap - w / 2
        mx, my = polyline_mid(pts)
        off = (lcx - mx, lcy - my)
        value = "<br>".join(h(l) for l in lines)
        lab = (lcx - w / 2, lcy - hh / 2, w, hh)
    wp = ""
    inner = pts[1:-1]
    if inner:
        wp = '<Array as="points">' + "".join('<mxPoint x="%g" y="%g"/>' % p for p in inner) + "</Array>"
    add(3, '<mxCell id="%s" value="%s" style="%s" edge="1" parent="1" source="%s" target="%s">'
           '<mxGeometry relative="1" as="geometry"><mxPoint x="%.1f" y="%.1f" as="offset"/>%s</mxGeometry></mxCell>'
        % (eid, attr(value), style, src, tgt, off[0], off[1], wp))
    edges.append((eid, src, tgt, pts, lab))


# =================================================================== LAYOUT
# Reading order is clockwise: row 1 runs left to right (IDLE, SEARCH phase, ORIENT), PATHFIND runs right to
# left, STORAGE runs bottom to top back to IDLE. STOP / FAULT sits in the middle as a shared sink: every
# fault edge is a short vertical line to it. Row 1 is centre-aligned on y = ROW1.
ROW1 = 250
TOP = 620            # top of the STORAGE and PATHFIND regions
BAR_Y, BAR_H = 440, 54

plain("title", 40, 20, 900, 30, "<b>AMU parcel-run FSM</b>", size=20, color="#000000")
plain("subtitle", 40, 52, 2200, 18,
      "One box per State in the Src/main.c switch (the ST_* enum). UML state-machine notation: "
      "trigger [guard] / effect. Terms follow GLOSSARY.md. A / B is the Layout learned by IDENTIFY.",
      size=11, color="#555555")

# ---- initial pseudo-state (top-left) and IDLE
pseudo("start", "ellipse", 182, 124, 16, "#000000")
state("idle", "1", 40, 180, 300, "IDLE", [
    "do / Kobuki_Drive(STOP)",
    "     Win_Sound_Tick()",
    "exit / layout = none",
    "       object.detected = 0",
    "       heading = 0",
    "       corridor = 0",
    "       Scan_Start(kobuki.angle)"], hgt=140)

# ---- SEARCH phase
composite("search_phase", 520, 140, 790, 190, "SEARCH (phase)")
state("search", "search_phase", 550, 210, 260, "SEARCH", [
    "do / Scan_Sample()",
    "     Kobuki_Rotate(150)  // ACW"], hgt=80)
state("identify", "search_phase", 980, 190, 300, "IDENTIFY", [
    "entry / Kobuki_Drive(STOP)",
    "do / layout = Layout_From_Range(",
    "         object.range_mm)",
    "     Kobuki_Update_LEDs(",
    "         A: RED, B: YELLOW, OFF)"], hgt=120)

# ---- STORAGE geometry (computed top-down so that every row stays centre-aligned)
SW = 310                          # width of a Storage State box
C1 = 130                          # left column: IDLE-facing States
GAP = 150
C2 = C1 + SW + GAP                # right column: sweep, reverse / approach, TURN
ST_RIGHT = 986                    # right edge of the STORAGE region
PF_X = ST_RIGHT + 40              # left edge of the PATHFIND region

body_deposit = [
    "entry / Kobuki_Drive(STOP)",
    "        Deposit_Scan_Start(kobuki.angle)",
    "        Kobuki_Update_LEDs(OFF, OFF)",
    "do / Deposit_Scan_Sample()",
    "     Kobuki_Rotate(50)  // ACW"]
body_align = [
    "do / turn = Deposit_Rod_Turn_Ticks()",
    "     Kobuki_Rotate(sign(turn) * speed)",
    "// coarse: 50 mm/s, 100 ticks",
    "// refined: 35 mm/s, 50 ticks",
    "// 1 tick = 0.01 deg"]
body_rturn = [
    "do / turn = Deposit_Refine_Turn_Ticks()",
    "     Kobuki_Rotate(sign(turn) * 35)",
    "// to the next of 26 steps,",
    "// -25 to +25 deg, 2 deg apart"]
body_rsample = [
    "entry / Kobuki_Drive(STOP)",
    "        Deposit_Refine_Settle()",
    "do / Kobuki_Drive(STOP)",
    "     Deposit_Refine_Sample()",
    "// settle 15 Ticks, then 3 readings",
    "// 5 Ticks apart; the median is kept"]
body_dist = [
    "entry / Kobuki_Drive(STOP)",
    "        deposit_settle_packets = 15",
    "        Kobuki_Update_LEDs(YELLOW, OFF)",
    "do / Deposit_Read_Range()",
    "// first 15 Ticks: Kobuki_Drive(STOP)",
    "//   and no reading (sonar catches up)"]
body_rev = [
    "do / Deposit_Read_Range()",
    "     Kobuki_Drive(-50)",
    "// back off past the target"]
body_app = [
    "do / Deposit_Read_Range()",
    "     Kobuki_Drive(35)",
    "// creep forward to the target"]
body_turn = [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: +131, B: -131)",
    "         // A: ACW, B: CW, to the Entrance",
    "do / Kobuki_Rotate_Angle(kobuki.angle)"]
body_drive_in = [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, 900)",
    "         // through the Entrance",
    "do / Kobuki_Drive_Distance(kobuki.distance)"]


def hh(body):
    return 26 + 6 + 14 * len(body) + 6


# row heights; rows are centre-aligned across the two columns
GROUP_Y = TOP + 48                                   # top of the DISTANCE ADJUST composite
yA_top = GROUP_Y + 24 + 40
yA = yA_top + hh(body_app) / 2                       # row A: APPROACH
yB = yA_top + hh(body_app) + 50 + hh(body_dist) / 2  # row B: DISTANCE | REVERSE
group_bottom = yB + hh(body_dist) / 2 + 24
yC = group_bottom + 80 + hh(body_align) / 2          # row C: ALIGN | REFINE_TURN
yD = yC + hh(body_align) / 2 + 90 + hh(body_rsample) / 2   # row D: DEPOSIT | REFINE_SAMPLE
yE = yD + hh(body_rsample) / 2 + 97 + hh(body_turn) / 2     # row E: DRIVE_IN | TURN
NOTE_Y = yE + hh(body_turn) / 2 + 36
REGION_H = NOTE_Y + 58 + 22 - TOP

composite("storage", 40, TOP, ST_RIGHT - 40, REGION_H, "STORAGE")
composite("dist_group", 110, GROUP_Y, C2 + SW + 20 - 110, group_bottom - GROUP_Y, "DISTANCE ADJUST",
          parent="storage", nested=True)

state("d_app", "dist_group", C2, yA - hh(body_app) / 2, SW, "DEPOSIT_APPROACH", body_app)
state("d_dist", "dist_group", C1, yB - hh(body_dist) / 2, SW, "DEPOSIT_DISTANCE", body_dist)
state("d_rev", "dist_group", C2, yB - hh(body_rev) / 2, SW, "DEPOSIT_REVERSE", body_rev)
state("d_align", "storage", C1, yC - hh(body_align) / 2, SW, "DEPOSIT_ALIGN", body_align)
state("d_rturn", "storage", C2, yC - hh(body_rturn) / 2, SW, "DEPOSIT_REFINE_TURN", body_rturn)
state("deposit", "storage", C1, yD - hh(body_deposit) / 2, SW, "DEPOSIT", body_deposit)
state("d_rsample", "storage", C2, yD - hh(body_rsample) / 2, SW, "DEPOSIT_REFINE_SAMPLE", body_rsample)
state("drive_in", "storage", C1, yE - hh(body_drive_in) / 2, SW, "DRIVE_IN", body_drive_in)
state("turn", "storage", C2, yE - hh(body_turn) / 2, SW, "TURN", body_turn)
note("storage_note", "storage", 70, NOTE_Y, ST_RIGHT - 40 - 60, 58, [
    "TURN ignores bumper and cliff (the turn is made beside the ledge). DRIVE_IN ignores the cliff; any bumper "
    "ends it in DEPOSIT. Every State here faults on B1 | wheeldrop; the three DISTANCE ADJUST States also fault "
    "on any bumper or cliff, or on a zero or maximum-range reading."])

# ---- PATHFIND: CORRIDOR_STOP | C2 | C3, vertical gaps >= 80 px
PC1 = PF_X + 30                    # CORRIDOR_STOP column
PC2 = PC1 + 230 + 40               # CORRIDOR_REVERSE / CORRIDOR_TURN column
PCV = PC2 + 320 + 30               # CORRIDOR_TURN -> DRIVE return line
PC3 = PCV + 150                    # DRIVE / REVERSE / ROTATE column
PF_RIGHT = PC3 + 320 + 150         # right edge of the PATHFIND region
composite("pathfind", PF_X, TOP, PF_RIGHT - PF_X, REGION_H, "PATHFIND")

Y_CREV, Y_CTURN, Y_REV, Y_ROT = 890, 1110, 960, 1200      # PATHFIND rows, vertical gaps >= 96 px
state("p_drive", "pathfind", PC3, 700, 320, "DRIVE", [
    "do / Kobuki_Drive(150)",
    "     // at the speed limit"], hgt=80)
pseudo("p_choice", "rhombus", PC2 + 160 - 14, 706, 28, "#ffffff")
state("p_creverse", "pathfind", PC2, Y_CREV, 320, "CORRIDOR_REVERSE", [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, -50)",
    "do / Kobuki_Drive_Distance(kobuki.distance)",
    "// bumper and cliff are ignored here"], hgt=94)
state("p_cturn", "pathfind", PC2, Y_CTURN, 320, "CORRIDOR_TURN", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: +90, B: -90)",
    "         // A: ACW, B: CW",
    "do / Kobuki_Rotate_Angle(kobuki.angle)",
    "exit / heading = kobuki.angle",
    "// bumper and cliff are ignored here"], hgt=124)
state("p_cstop", "pathfind", PC1, yE - 27, 230, "CORRIDOR_STOP", [
    "entry / Kobuki_Drive(STOP)"], hgt=54)
state("p_reverse", "pathfind", PC3, Y_REV, 320, "REVERSE", [
    "entry / Kobuki_Drive_Distance_Setpoint(",
    "         kobuki.distance, -50)",
    "do / Kobuki_Drive_Distance(kobuki.distance)",
    "// bumper and cliff are ignored here"], hgt=94)
state("p_rotate", "pathfind", PC3, Y_ROT, 320, "ROTATE", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, pathfind_recover_deg)",
    "         // latched in DRIVE: wall A -5, B +5;",
    "         // ledge A +5, B -5",
    "do / Kobuki_Rotate_Angle(kobuki.angle)",
    "// bumper and cliff are ignored here"], hgt=124)
note("pathfind_note", "pathfind", PC2, NOTE_Y, PC3 + 320 - PC2, 58, [
    "Runs twice: corridor = 1 ends in CORRIDOR_REVERSE, then CORRIDOR_TURN; corridor = 2 ends in "
    "CORRIDOR_STOP. A corridor ends on the front cliff, not on distance. DRIVE ignores the ledge-side "
    "bumper and the storage-wall-side cliff; REVERSE, ROTATE and the CORRIDOR_ States ignore bumper and "
    "cliff. Only B1 | wheeldrop faults here."])

# ---- ORIENT (centred over DRIVE) and the FAULT bar
ORIENT_X = PC3 + 160 - 150
state("orient", "1", ORIENT_X, 190, 300, "ORIENT", [
    "entry / Kobuki_Rotate_Angle_Setpoint(",
    "         kobuki.angle, A: -75, B: +108)",
    "         // A: CW, B: ACW",
    "do / Kobuki_Rotate_Angle(kobuki.angle)"], hgt=120)
DRIVE_CX = PC3 + 160
BAR_X = 520                                    # left edge aligned with the SEARCH phase
state("fault", "1", BAR_X, BAR_Y, DRIVE_CX - 40 - BAR_X, "STOP / FAULT", [
    "entry / Kobuki_Drive(STOP)"], kind="fault", hgt=BAR_H)
BAR_BOTTOM = BAR_Y + BAR_H
BAR_MID = BAR_Y + BAR_H / 2

# ===================================================================== EDGES
# main flow: start -> IDLE -> SEARCH -> IDENTIFY -> ORIENT -> DRIVE -> ...
edge("e_start", "start", "idle", [(190, 140), (190, 180)])
edge("e_idle_search", "idle", "search", [(340, ROW1), (550, ROW1)],
     lines=["B0", "[kobuki.button & 0x01", "&& !DEPOSIT_TEST_MODE]"], seg=0, side="above")
edge("e_search_identify", "search", "identify", [(810, ROW1), (980, ROW1)],
     lines=["1: [object.detected != 0]"], seg=0, side="above")
edge("e_identify_orient", "identify", "orient", [(1280, ROW1), (ORIENT_X, ROW1)],
     lines=["ε"], seg=0, side="above")
edge("e_orient_drive", "orient", "p_drive", [(DRIVE_CX, 310), (DRIVE_CX, 700)],
     lines=["kobuki.rotation_complete", "/ heading = kobuki.angle,", "corridor = 1"], seg=0, side="right", gap=8)

# fault edges (red): one short vertical line per region into the shared STOP / FAULT bar
SEARCH_X, PHASE_CX = cx("search"), 520 + 790 / 2
FAULT_LBL = ["B1 | wheeldrop", "| bumper | cliff"]
edge("f_search_giveup", "search", "fault", [(SEARCH_X, 290), (SEARCH_X, BAR_Y)], color=RED, sw=1.25,
     lines=["2: [scan_deg >= 360]", "/ LED1 green"], seg=0, side="right", gap=8)
edge("f_search_phase", "search_phase", "fault", [(PHASE_CX, 330), (PHASE_CX, BAR_Y)], color=RED, sw=1.25,
     lines=FAULT_LBL, seg=0, side="right", gap=8)
edge("f_orient", "orient", "fault", [(ORIENT_X + 30, 310), (ORIENT_X + 30, BAR_Y)], color=RED, sw=1.25,
     lines=FAULT_LBL, seg=0, side="right", gap=8)
PF_FAULT_X = PC2 + 100
edge("f_pathfind", "pathfind", "fault", [(PF_FAULT_X, TOP), (PF_FAULT_X, BAR_BOTTOM)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop"], seg=0, side="left", gap=8)

# STORAGE fault edges (all upward, labels to the left, except the right-hand lane which has no room on its left)
F_DIST1, F_DIST2 = 660, 790
F_ST, F_SWEEP = 942, 964
edge("f_dist_any", "dist_group", "fault", [(F_DIST1, GROUP_Y), (F_DIST1, BAR_BOTTOM)], color=RED, sw=1.25,
     lines=FAULT_LBL, seg=0, side="left", gap=8, shift=22)
edge("f_dist_range", "dist_group", "fault", [(F_DIST2, GROUP_Y), (F_DIST2, BAR_BOTTOM)], color=RED, sw=1.25,
     lines=["[Deposit_Read_Range()", "is false]", "/ LED1 red"], seg=0, side="left", gap=8, shift=8)
edge("f_storage", "storage", "fault", [(F_ST, TOP), (F_ST, BAR_BOTTOM)], color=RED, sw=1.25,
     lines=["B1 | wheeldrop"], seg=0, side="left", gap=8, shift=22)
sweep_y = yD - 24
SWEEP_LBL = ["[Deposit_Refine_Sample()", "&& !Deposit_Refine_Next()", "&& !Deposit_Refine_Finish()]",
             "/ LED1 red"]
edge("f_sweep", "d_rsample", "fault",
     [(right("d_rsample"), sweep_y), (F_SWEEP, sweep_y), (F_SWEEP, BAR_BOTTOM)], color=RED, sw=1.25,
     lines=SWEEP_LBL, seg=1, side="right", gap=8, at=lbl_right_of(F_SWEEP, 550, SWEEP_LBL))

# returns to IDLE
FAULT_IDLE_X = 430
edge("e_fault_idle", "fault", "idle", [(BAR_X, BAR_MID), (FAULT_IDLE_X, BAR_MID), (FAULT_IDLE_X, 300), (340, 300)],
     lines=["ε"], seg=0, side="below", gap=4)

# PATHFIND
CHOICE_L, CHOICE_R = PC2 + 160 - 14, PC2 + 160 + 14
edge("p_drive_choice", "p_drive", "p_choice", [(PC3, 720), (CHOICE_R, 720)],
     lines=["1: front cliff", "[kobuki.cliff & 0x02]"], seg=0, side="below")
edge("p_choice_creverse", "p_choice", "p_creverse", [(PC2 + 160, 734), (PC2 + 160, Y_CREV)],
     lines=["[corridor == 1]"], seg=0, side="right", gap=8)
CSTOP_X = cx("p_cstop")
edge("p_choice_stop", "p_choice", "p_cstop", [(CHOICE_L, 720), (CSTOP_X, 720), (CSTOP_X, gy("p_cstop"))],
     lines=["[corridor == 2]"], seg=0, side="below")
edge("p_creverse_cturn", "p_creverse", "p_cturn", [(PC2 + 160, Y_CREV + 94), (PC2 + 160, Y_CTURN)],
     lines=["kobuki.distance_complete"], seg=0, side="right", gap=8)
edge("p_cturn_drive", "p_cturn", "p_drive",
     [(PC2 + 320, Y_CTURN + 62), (PCV, Y_CTURN + 62), (PCV, 772), (PC3, 772)],
     lines=["kobuki.rotation_complete", "/ corridor = 2"], seg=1, side="right", gap=8)
edge("p_drive_reverse", "p_drive", "p_reverse", [(DRIVE_CX, 780), (DRIVE_CX, Y_REV)],
     lines=["2: storage-wall bumper", "[A: kobuki.bumper & 0x04,", " B: kobuki.bumper & 0x01]",
            "/ recover = A -5 | B +5", " ", "3: ledge cliff", "[A: kobuki.cliff & 0x01,",
            " B: kobuki.cliff & 0x04]", "/ recover = A +5 | B -5"], seg=0, side="right", gap=10)
edge("p_reverse_rotate", "p_reverse", "p_rotate", [(DRIVE_CX, Y_REV + 94), (DRIVE_CX, Y_ROT)],
     lines=["kobuki.distance_complete"], seg=0, side="right", gap=8)
LOOP_X = PC3 + 320 + 110
edge("p_rotate_drive", "p_rotate", "p_drive", [(PC3 + 320, Y_ROT + 62), (LOOP_X, Y_ROT + 62), (LOOP_X, 740), (PC3 + 320, 740)],
     lines=["kobuki.", "rotation_complete"], seg=0, side="above", gap=6)
edge("p_stop_turn", "p_cstop", "turn", [(PC1, yE), (right("turn"), yE)],
     lines=["ε"], seg=0, side="below")

# STORAGE (flow runs bottom to top; every vertical label sits beside its line, left of an upward one)
C1R, C2L = C1 + SW, C2
G_MID = (C1R + C2L) / 2
edge("e_turn_drivein", "turn", "drive_in", [(C2L, yE), (C1R, yE)],
     lines=["kobuki.", "rotation_complete"], seg=0, side="below")
edge("e_drivein_deposit_bump", "drive_in", "deposit",
     [(C1 + 130, gy("drive_in")), (C1 + 130, bottom("deposit"))],
     lines=["1: any bumper", "[kobuki.bumper != 0]", "/ Kobuki_Drive(STOP)"], seg=0, side="left", gap=8)
edge("e_drivein_deposit_done", "drive_in", "deposit",
     [(C1 + 270, gy("drive_in")), (C1 + 270, bottom("deposit"))],
     lines=["2: kobuki.", "distance_complete"], seg=0, side="left", gap=8)
edge("e_deposit_align", "deposit", "d_align", [(C1 + 210, gy("deposit")), (C1 + 210, bottom("d_align"))],
     lines=["[scan_deg >= 360,", "deposit_rod.detected]", "/ Kobuki_Drive(STOP)"], seg=0, side="left", gap=8)

# DEPOSIT <-> IDLE lanes on the left margin: T goes down on the outer lane, N comes back up on the inner one
LANE_T, LANE_N, JOG_Y = 60, 85, 590
N_X, D_X = 250, 310
yN, yT = yD - 24, yD + 24
T_LBL = ["B0", "[kobuki.button & 0x01", "&& DEPOSIT_TEST_MODE]"]
N_LBL = ["[scan_deg >= 360,", "!deposit_rod.detected]", "/ Kobuki_Drive(STOP),", "LED1 red"]
edge("e_idle_deposit", "idle", "deposit", [(LANE_T, 320), (LANE_T, yT), (C1, yT)],
     lines=T_LBL, seg=0, side="right", gap=8, at=lbl_right_of(LANE_T, 365, T_LBL))
edge("e_deposit_idle", "deposit", "idle", [(C1, yN), (LANE_N, yN), (LANE_N, JOG_Y), (N_X, JOG_Y), (N_X, 320)],
     lines=N_LBL, seg=3, side="left", gap=8, at=lbl_left_of(N_X, 450, N_LBL))

edge("e_align_rturn", "d_align", "d_rturn", [(C1R, yC - 18), (C2L, yC - 18)],
     lines=["[!deposit_refined &&", "|turn| <= 100]", "/ Kobuki_Drive(STOP),", "Deposit_Refine_Start()"],
     seg=0, side="above")
edge("e_align_dist", "d_align", "d_dist", [(C1 + 290, gy("d_align")), (C1 + 290, bottom("d_dist"))],
     lines=["[deposit_refined &&", "|turn| <= 50]", "/ Kobuki_Drive(STOP)"], seg=0, side="left", gap=8)
edge("e_rturn_rsample", "d_rturn", "d_rsample", [(C2 + 250, bottom("d_rturn")), (C2 + 250, gy("d_rsample"))],
     lines=["[|turn| <= 50]"], seg=0, side="right", gap=8)
edge("e_rsample_rturn", "d_rsample", "d_rturn", [(C2 + 100, gy("d_rsample")), (C2 + 100, bottom("d_rturn"))],
     lines=["[Deposit_Refine_Sample()", "&& Deposit_Refine_Next()]"], seg=0, side="left", gap=8)
edge("e_rsample_align", "d_rsample", "d_align",
     [(C2L, yD + 30), (G_MID, yD + 30), (G_MID, yC + 24), (C1R, yC + 24)],
     lines=["[Deposit_Refine_Sample()", "&& !Deposit_Refine_Next()", "&& Deposit_Refine_Finish()]"],
     seg=1, side="left", gap=8)

# DISTANCE ADJUST: DISTANCE is the hub; IDLE leaves straight up, REVERSE sits to its right, APPROACH above REVERSE
E_B, E_C = C1 + 230, C1 + 280                    # elbow lanes on DISTANCE's top edge, right of the IDLE line
y_app1, y_app2 = yA - 18, yA + 18
D_LBL = ["[400 <= deposit_range_mm", "<= 420]", "/ LED1 green,", "Win_Sound_Start()"]
edge("e_dist_idle", "d_dist", "idle", [(D_X, gy("d_dist")), (D_X, 320)],
     lines=D_LBL, seg=0, side="right", gap=8, at=lbl_right_of(D_X, 535, D_LBL))
edge("e_dist_rev", "d_dist", "d_rev", [(C1R, yB - 16), (C2L, yB - 16)],
     lines=["[deposit_range_mm", "< 400]"], seg=0, side="above")
edge("e_rev_dist", "d_rev", "d_dist", [(C2L, yB + 16), (C1R, yB + 16)],
     lines=["[deposit_range_mm", ">= 510]"], seg=0, side="below")
edge("e_dist_app", "d_dist", "d_app", [(E_B, gy("d_dist")), (E_B, y_app1), (C2L, y_app1)],
     lines=["[deposit_range_mm", "> 420]"], seg=1, side="above")
edge("e_app_dist", "d_app", "d_dist", [(C2L, y_app2), (E_C, y_app2), (E_C, gy("d_dist"))],
     lines=["[deposit_range_mm", "<= 420]"], seg=0, side="below")

# CSTOP -> TURN is drawn above with the PATHFIND edges (p_stop_turn)

# ===================================================================== LEGEND
LY = TOP + REGION_H + 50
pseudo("lg_dot", "ellipse", 44, LY + 6, 16, "#000000")
plain("lg_dot_t", 70, LY + 6, 150, 18, "start (initial State)")
pseudo("lg_dia", "rhombus", 230, LY, 28, "#ffffff")
plain("lg_dia_t", 266, LY + 6, 350, 18, "choice: one trigger, the guard on each edge picks the way")

for i, (cid, label, fill, stroke, dash) in enumerate([
        ("lg_st", "state", "#ffffff", BLACK, ""),
        ("lg_fa", "fault", "#f8cecc", RED, ""),
        ("lg_su", "composite (phase)", "#eeeeee", "#666666", "")]):
    x = [660, 770, 880][i]
    w = [90, 90, 170][i]
    add(1, '<mxCell id="%s" value="%s" style="rounded=1;absoluteArcSize=1;arcSize=12;html=1;fontSize=11;'
           'fontFamily=Helvetica;fillColor=%s;strokeColor=%s;strokeWidth=1.5;%s" vertex="1" parent="1">'
           '<mxGeometry x="%d" y="%d" width="%d" height="30" as="geometry"/></mxCell>'
        % (cid, attr(label), fill, stroke, dash, x, LY - 1, w))
plain("lg_red", 1090, LY + 6, 800, 18,
      '<font color="%s">red edge</font> = fault: the State (or every State in the region) leaves for STOP / FAULT' % RED)

# ===================================================================== NOTES
NY = LY + 70
NH = 380                       # height of the notes text boxes
NW = 420
cols = [40, 490, 940, 1390, 1840]

n1 = ("<b>How to read it</b><br>A State is a rounded box: its name, then its actions. <b>entry /</b> runs once on "
      "entering, <b>do /</b> on every Tick in which no edge fired, <b>exit /</b> once on leaving. Edges read: "
      "trigger [guard] / effect. A State's edges are checked in priority order, the red fault edge first, and "
      "the first true one fires; a transition runs exit(), then the effect, then entry() of the next State. "
      "1:, 2:, 3: number the edges that share a source.<br><br>"
      "<b>ε</b> = no trigger: the edge is taken on the next Tick. One Tick = one Kobuki packet: Kobuki_Read() "
      "runs first, then the state switch.<br><br>"
      "Angles are degrees: + = ACW (left), - = CW (right). Gyro ticks are 0.01 degree. <b>heading</b> = "
      "kobuki.angle, latched when ORIENT completes and when CORRIDOR_TURN is left.<br><br>"
      "Setpoints (Kobuki_Rotate_Angle_Setpoint, Kobuki_Drive_Distance_Setpoint) are one-shot entry actions. "
      "kobuki.rotation_complete and kobuki.distance_complete are copied at the top of the Tick by "
      "Kobuki_Read(), so a State sees them one Tick after its last do / call.<br><br>"
      "<b>Build option</b><br>DEPOSIT_TEST_MODE is a build-time constant (default 0), so only one of the two B0 "
      "edges out of IDLE exists in a given build. With it set to 1, the AMU also enters DEPOSIT once after the "
      "first Kobuki packet at boot, for bench-testing Deposit on its own.")

n2 = ("<b>Layouts</b><br>A / B is the Layout saved by IDENTIFY, never a live reading. Layout A: bollards A1/A2 "
      "block the left/top route; ORIENT turns clockwise to the bottom corridor; the storage wall is on the "
      "AMU's left and the ledge on its right. Layout B is the mirror image: bollards B1/B2 block the "
      "bottom/right route; ORIENT turns anticlockwise to the left corridor.<br><br>"
      "<b>Sensors</b><br>Bumper / cliff bits: right = 0x01, centre = 0x02, left = 0x04. B0 = kobuki.button "
      "&amp; 0x01 (start). B1 = kobuki.button &amp; 0x02 (stop). Fault_B1_Wheeldrop() = B1 | "
      "kobuki.wheeldrop != 0. Fault_Any() = that | any bumper | any cliff.<br><br>"
      "<b>Faults</b><br>B1 | wheeldrop leaves every State except IDLE and STOP / FAULT. SEARCH, IDENTIFY, "
      "ORIENT and the three DISTANCE ADJUST States also fault on any bumper or cliff. Bumpers and cliffs are "
      "expected in PATHFIND and in the rest of STORAGE, so those regions have one red edge each, for "
      "B1 | wheeldrop only. Three faults are State-specific: the SEARCH give-up (a full turn without a "
      "bollard), a refine sweep with no echo, and a zero or maximum-range reading while adjusting the "
      "distance. STOP / FAULT stops the motors and returns to IDLE on the next Tick.")

n3 = ("<b>Search phase</b><br>SEARCH turns anticlockwise on the spot from B0 and reads the ultrasonic once per "
      "Tick. <b>scan_deg</b> is the net turn since B0 in degrees, ACW positive, summed from gyro changes so "
      "it does not wrap at 180; it only drives the give-up (scan_deg >= 360: LED1 green, then FAULT). A "
      "Detection is range_mm &lt; 750. IDENTIFY stops on the first Detection, then layout = "
      "Layout_From_Range(range_mm): B when range_mm &lt; 575, otherwise A. LED1 red for A, yellow for B. "
      "There is no centring: ORIENT turns from wherever SEARCH stopped.<br><br>"
      "<b>IDLE reset</b><br>Leaving IDLE (B0) clears layout, object.detected, heading and corridor, and "
      "zeroes the scan with Scan_Start(). It also cuts any win sound still playing with Win_Sound_Stop(). "
      "DEPOSIT restarts the same scan counter for its own full circle.<br><br>"
      "<b>Calibration (layout split, detect range)</b><br>The split is a calibration value (575 mm now). "
      "IDENTIFY reads the first in-range reading, taken as the sonar beam first reaches the bollard, so "
      "calibrate the same way: turn the AMU anticlockwise onto B1, then onto A1, and read "
      "robot.object.range_mm when SEARCH stops; the split goes between them. The detect range must stay "
      "above the A1 reading and below the nearest wall.")

n4 = ("<b>Deposit</b><br>DEPOSIT turns anticlockwise through a full circle (scan_deg >= 360) at 50 mm/s and "
      "keeps the closest valid echo (not 0, at most DEPOSIT_SCAN_MAX_RANGE_MM; 5000 mm is no echo) with its "
      "gyro heading. With no echo it lights LED1 red and returns to IDLE.<br><br>"
      "DEPOSIT_ALIGN turns the short way to the saved heading. The first time, within 100 ticks, it starts "
      "the refine sweep; once the heading is refined, within 50 ticks, it goes to DEPOSIT_DISTANCE.<br><br>"
      "The sweep visits 26 headings from -25 to +25 deg around the coarse heading, 2 deg apart. "
      "DEPOSIT_REFINE_TURN turns to a step at 35 mm/s; DEPOSIT_REFINE_SAMPLE stops, waits 15 Ticks, takes 3 "
      "readings 5 Ticks apart and keeps the median. After the last step, the centre of the widest run of "
      "steps within 10 mm of the closest median becomes the heading (ties: nearest the coarse heading). No "
      "valid median faults with LED1 red.<br><br>"
      "DEPOSIT_DISTANCE holds still for 15 Ticks (about 300 ms) so the 10 Hz sonar catches up, then reads "
      "deposit_range_mm against the target 410 mm +/- 10 mm: too close backs off in DEPOSIT_REVERSE until "
      "510 mm (a 100 mm margin) at 50 mm/s, too far creeps in DEPOSIT_APPROACH at 35 mm/s until 420 mm, so "
      "the last correction is always made moving forward. Inside the band it lights LED1 green, starts the "
      "win sound with Win_Sound_Start() and returns to IDLE, which sends one note per Tick with "
      "Win_Sound_Tick(); LED1 is yellow while adjusting.")

n5 = ("<b>Constants (Src/Helper/helper.h)</b><br>Scan: turn speed 150, give-up after a full turn of 360. "
      "Layout split 575 mm (B below, A otherwise). Orient A -75 / B +108. Pathfind: drive speed 150, reverse "
      "50 mm, wall-hit nudge A -5 / B +5 (ledge cliff: the opposite), corridor turn A +90 / B -90. Storage: "
      "turn A +131 / B -131, drive-in 900 mm. Detect range 750 mm (Src/Ultrasonic/ultrasonic.h). The Kobuki "
      "speed limit is 150 mm/s.<br><br>"
      "Deposit: scan speed 50, align tolerance 100 ticks, refine sweep +/-25 deg in 2 deg steps at 35 with a "
      "50 tick tolerance, 10 mm range band, 5 Ticks between readings, settle 15 Ticks, target 410 mm, "
      "tolerance 10 mm, reverse margin 100 mm, reverse speed 50, approach speed 35. DEPOSIT_TEST_MODE 0.<br><br>"
      "<b>Helpers (Src/Helper/helper.c)</b><br>Layout_From_Range(range_mm) is the single split; "
      "Layout_Pick(a, b) picks a value by Layout. Scan_Sample() = Scan_Update_Turn(kobuki.angle) + "
      "Ultrasonic_Detect(&amp;object); Scan_Gave_Up() = scan_deg >= 360. Corridor_Take_Wall_Hit() and "
      "Corridor_Take_Ledge_Hit() test the Layout's side and latch pathfind_recover_deg. Deposit_*() keep the "
      "closest echo, drive the refine sweep and read the live range.")

for i, t in enumerate([n1, n2, n3, n4, n5]):
    # the notes are already HTML; store them as-is
    box("note_col%d" % (i + 1), "1", cols[i], NY, NW, NH)
    add(1, '<mxCell id="note_col%d" value="%s" style="text;html=1;align=left;verticalAlign=top;spacing=0;'
           'fontSize=11;fontFamily=Helvetica;fontColor=#333333;whiteSpace=wrap;" vertex="1" parent="1">'
           '<mxGeometry x="%d" y="%d" width="%d" height="%d" as="geometry"/></mxCell>'
        % (i + 1, attr(t), cols[i], NY, NW, NH))


# ===================================================================== LAYOUT LINT
def overlap(a, b, pad=0.0):
    return not (a[0] + a[2] <= b[0] + pad or b[0] + b[2] <= a[0] + pad or
                a[1] + a[3] <= b[1] + pad or b[1] + b[3] <= a[1] + pad)


def seg_hits_rect(p, q, r, pad=0.0):
    """True when the axis-parallel segment p-q passes through the interior of rectangle r."""
    x1, y1, x2, y2 = r[0] + pad, r[1] + pad, r[0] + r[2] - pad, r[1] + r[3] - pad
    if p[1] == q[1]:
        a, b = sorted((p[0], q[0]))
        return y1 < p[1] < y2 and a < x2 and b > x1
    a, b = sorted((p[1], q[1]))
    return x1 < p[0] < x2 and a < y2 and b > y1


def lint():
    problems = []
    ids = list(obst)
    # 1. boxes must not overlap
    for i, a in enumerate(ids):
        for b in ids[i + 1:]:
            if overlap(obst[a], obst[b]):
                problems.append("boxes overlap: %s / %s" % (a, b))
    # 2. every State sits inside its composite
    for cid, (x, y, w, hh_, parent) in reg.items():
        if parent in reg and cid in obst:
            px, py, pw, ph, _ = reg[parent]
            if x < px or y < py or x + w > px + pw or y + hh_ > py + ph:
                problems.append("%s sticks out of %s" % (cid, parent))
    # 3. per edge: label clear of boxes, other labels and every line; line clear of foreign boxes
    for eid, src, tgt, pts, lab in edges:
        own = {src, tgt}
        segs = list(zip(pts, pts[1:]))
        for p, q in segs:
            for bid, r in obst.items():
                if bid in own:
                    continue
                if seg_hits_rect(p, q, r):
                    problems.append("edge %s passes through %s" % (eid, bid))
        if lab:
            for bid, r in obst.items():
                if overlap(lab, r, pad=0.5):
                    problems.append("label of %s overlaps %s" % (eid, bid))
            for oid, _, _, opts, olab in edges:
                if olab and oid > eid and overlap(lab, olab, pad=0.5):
                    problems.append("labels of %s and %s overlap" % (eid, oid))
                for p, q in zip(opts, opts[1:]):
                    if seg_hits_rect(p, q, lab, pad=-1.0):
                        problems.append("label of %s sits on a line of %s" % (eid, oid))
    # 4. edge crossings and overlaps between different edges
    for i, (e1, s1, t1, p1, _) in enumerate(edges):
        for e2, s2, t2, p2, _ in edges[i + 1:]:
            for a, b in zip(p1, p1[1:]):
                for c, d in zip(p2, p2[1:]):
                    ah, ch = a[1] == b[1], c[1] == d[1]
                    if ah != ch:
                        hs, vs = ((a, b), (c, d)) if ah else ((c, d), (a, b))
                        hx = sorted((hs[0][0], hs[1][0]))
                        vy = sorted((vs[0][1], vs[1][1]))
                        if hx[0] < vs[0][0] < hx[1] and vy[0] < hs[0][1] < vy[1]:
                            problems.append("edges %s and %s cross" % (e1, e2))
                    elif ah and a[1] == c[1] and min(max(a[0], b[0]), max(c[0], d[0])) - max(min(a[0], b[0]), min(c[0], d[0])) > 1:
                        problems.append("edges %s and %s overlap on a line" % (e1, e2))
                    elif not ah and a[0] == c[0] and min(max(a[1], b[1]), max(c[1], d[1])) - max(min(a[1], b[1]), min(c[1], d[1])) > 1:
                        problems.append("edges %s and %s overlap on a line" % (e1, e2))
    return problems


# ===================================================================== OUTPUT
PAGE_W, PAGE_H = PF_RIGHT + 40, NY + NH + 40
cells.sort(key=lambda c: c[0])  # stable: composites, states, bodies/notes, edges
xml = ('<mxfile host="app.diagrams.net" agent="amu-fsm-generator" version="24.7.5"><diagram id="amu_fsm" '
       'name="AMU FSM"><mxGraphModel dx="1400" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" '
       'connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="%d" pageHeight="%d" math="0" '
       'shadow="0"><root><mxCell id="0"/><mxCell id="1" parent="0"/>' % (PAGE_W, PAGE_H))
xml += "".join(c[1] for c in cells)
xml += "</root></mxGraphModel></diagram></mxfile>"
open(OUT, "w").write(xml)
print("wrote", OUT, len(cells), "cells")
issues = lint()
for p in issues:
    print("  lint:", p)
print("layout lint: %d problems" % len(issues))
