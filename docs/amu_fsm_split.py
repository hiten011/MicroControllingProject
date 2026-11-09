#!/usr/bin/env python3
"""Split the AMU FSM diagram into six compact, portrait SVGs that are easy to read and paste into a report (docs/fsm/*.svg).
The SVGs are real vector graphics (plain <text>, no foreignObject, no dark-mode styles), so they import into
Word, PowerPoint, LaTeX and Inkscape and never pixelate.

Usage:  python3 docs/amu_fsm_split.py [--no-render] [--keep-drawio] [--png] [out_dir]
        (default out_dir: docs/fsm; needs `node` + `playwright` for the SVGs; --png writes PNGs instead, see docs/amu_fsm_render.js)

docs/amu_fsm.drawio stays the single source of truth. This script reads it and builds one smaller
.drawio per view, re-using the real State boxes, bodies and edges with their exact text and
coordinates, so a change made in docs/amu_fsm_gen.py shows up here after regenerating. Only the pieces
that cannot be copied are drawn here:

  * a dashed grey box is a State (or IDLE / STOP / FAULT) that lives in another picture; the edge
    that leaves the picture ends on it, with its original label;
  * a view that cuts through STORAGE or PATHFIND redraws that composite around the States it shows;
  * 01_overview is drawn from scratch (phases only), because the full diagram has no such level;
  * every view re-lays the States out in a tight, mostly vertical stack (the full diagram is one wide
    canvas), and routes the edges again with the same labels, so the pictures stay small when pasted.

The views:  01_overview  02_search_orient  03_pathfind  04_storage_entry_scan
            05_storage_align_refine  06_storage_distance_adjust
"""
import copy
import html
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

try:
    from PIL import ImageFont
except ImportError:  # label widths are then estimated
    ImageFont = None

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "amu_fsm.drawio")
RENDER = os.path.join(HERE, "amu_fsm_render.js")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
OUT = args[0] if args else os.path.join(HERE, "fsm")
RENDER_PNG = "--no-render" not in sys.argv

BLACK, RED, GREY = "#000000", "#b85450", "#6b6b6b"
LINE_H = 13.5

# ------------------------------------------------------------------ label measuring (as in amu_fsm_gen.py)
def _font():
    if ImageFont is None:
        return None
    for p in ("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
              "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "/Library/Fonts/Arial.ttf",
              "/System/Library/Fonts/Supplemental/Arial.ttf", "C:/Windows/Fonts/arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(p, 11)
        except OSError:
            continue
    return None


SANS = _font()


def tw(s):
    return SANS.getlength(s) if SANS else 5.6 * len(s)


def lines_of(value):
    return [html.unescape(l) for l in value.split("<br>")]


def lsize(lines):
    return max(tw(l) for l in lines) + 8, LINE_H * len(lines) + 4


def mid(pts):
    segs = [abs(b[0] - a[0]) + abs(b[1] - a[1]) for a, b in zip(pts, pts[1:])]
    half, acc = sum(segs) / 2, 0.0
    for (a, b), d in zip(zip(pts, pts[1:]), segs):
        if acc + d >= half:
            t = (half - acc) / d if d else 0
            return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        acc += d
    return pts[-1]


# ------------------------------------------------------------------------------------- the source diagram
src_root = ET.parse(SRC).getroot()
S = {c.get("id"): c for c in src_root.iter("mxCell")}


def sgeo(cid):
    g = S[cid].find("mxGeometry")
    return float(g.get("x", 0)), float(g.get("y", 0)), float(g.get("width", 0)), float(g.get("height", 0))


def sabs(cid):
    """Absolute (x, y, w, h) of a source vertex."""
    x, y, w, h = sgeo(cid)
    p = S[cid].get("parent")
    while p not in (None, "0", "1"):
        px, py, _, _ = sgeo(p)
        x, y, p = x + px, y + py, S[p].get("parent")
    return x, y, w, h


def val(eid):
    return S[eid].get("value")


def sstyle(eid, key):
    for kv in S[eid].get("style", "").split(";"):
        if kv.startswith(key + "="):
            return kv.split("=", 1)[1]
    return None


def named(cid):
    return html.unescape(S[cid].get("value") or cid)


# ------------------------------------------------------------------------------------------ view builder
GHOST = ("rounded=1;absoluteArcSize=1;arcSize=12;html=1;whiteSpace=wrap;dashed=1;dashPattern=6 4;"
         "fillColor=#fafafa;strokeColor=#888888;strokeWidth=1.5;fontColor=#555555;fontSize=12;fontFamily=Helvetica;")
GHOST_FAULT = GHOST.replace("fillColor=#fafafa;strokeColor=#888888", "fillColor=#f8cecc;strokeColor=#b85450") \
                   .replace("fontColor=#555555", "fontColor=#b85450")
PLAIN = "text;html=1;align=left;verticalAlign=top;spacing=0;fontFamily=Helvetica;whiteSpace=wrap;"
NOTE = ("shape=note;size=10;whiteSpace=wrap;html=1;backgroundOutline=1;fillColor=#ffffff;strokeColor=#808080;"
        "strokeWidth=1;align=left;verticalAlign=top;spacingLeft=6;spacingRight=6;spacingTop=3;fontSize=10.5;"
        "fontFamily=Helvetica;fontColor=#444444;")


class View:
    def __init__(self, name, title, subtitle, tx, ty, width=1100):
        self.name = name
        self.under, self.boxes, self.edges, self.over = [], [], [], []   # z-order: under < boxes < edges < over
        self.reg = {}
        self.text(tx, ty, width, 28, "<b>%s</b>" % html.escape(title), 20, "#000000")
        self.text(tx, ty + 32, width, 34, html.escape(subtitle), 11, "#555555")

    # -- plumbing
    def _cell(self, bucket, cid, value, style, x, y, w, h, parent="1", vertex=True):
        c = ET.Element("mxCell", id=cid, value=value, style=style, vertex="1", parent=parent)
        ET.SubElement(c, "mxGeometry", x="%g" % x, y="%g" % y, width="%g" % w, height="%g" % h).set("as", "geometry")
        bucket.append(c)
        self.reg[cid] = (x, y, w, h)
        return c

    def text(self, x, y, w, h, value, size=11, color="#333333"):
        self._cell(self.over, "t%d" % len(self.over), value, PLAIN + "fontSize=%g;fontColor=%s;" % (size, color),
                   x, y, w, h)

    def note(self, x, y, w, h, value):
        self._cell(self.over, "n%d" % len(self.over), value, NOTE, x, y, w, h)

    # -- boxes
    def composite(self, cid, like, x, y, w, h, name=None):
        c = copy.deepcopy(S[like])
        c.set("id", cid)
        c.set("parent", "1")
        if name:
            c.set("value", name)
        g = c.find("mxGeometry")
        for k, v in zip("x y width height".split(), (x, y, w, h)):
            g.set(k, "%g" % v)
        self.under.append(c)
        self.reg[cid] = (x, y, w, h)

    def state(self, cid, at=None):
        """A real State box with its body, at its absolute position (or at `at` = (x, y))."""
        x, y, w, h = sabs(cid)
        if at:
            x, y = at
        c = copy.deepcopy(S[cid])
        c.set("parent", "1")
        g = c.find("mxGeometry")
        g.set("x", "%g" % x)
        g.set("y", "%g" % y)
        self.boxes.append(c)
        self.reg[cid] = (x, y, w, h)
        body = cid + "_body"
        if body in S:
            self.boxes.append(copy.deepcopy(S[body]))

    def raw(self, cid):
        """Any other source vertex (start dot, choice diamond) at its absolute position."""
        x, y, w, h = sabs(cid)
        c = copy.deepcopy(S[cid])
        c.set("parent", "1")
        g = c.find("mxGeometry")
        g.set("x", "%g" % x)
        g.set("y", "%g" % y)
        self.boxes.append(c)
        self.reg[cid] = (x, y, w, h)

    def ghost(self, gid, x, y, w, h, name, where, fault=False):
        v = "<b>%s</b><br><font style=\"font-size:10px\">%s</font>" % (html.escape(name), html.escape(where))
        self._cell(self.boxes, gid, v, GHOST_FAULT if fault else GHOST, x, y, w, h)

    def ghost_of(self, cid, where, at=None, h=None):
        x, y, w, h0 = sabs(cid)
        if at:
            x, y = at
        self.ghost(cid, x, y, w, h or h0, named(cid), where)

    def fault_bar(self, x, y, w, h):
        """The red STOP / FAULT State, any size."""
        self.composite("fault", "fault", x, y, w, h)
        b = copy.deepcopy(S["fault_body"])
        b.find("mxGeometry").set("width", "%g" % w)
        self.boxes.append(b)

    def pt(self, cid, side, off=None):
        """Point on a registered box: side t/b/l/r; off = x (t/b) or y (l/r), default the middle."""
        x, y, w, h = self.reg[cid]
        if side == "t":
            return (x + w / 2 if off is None else off, y)
        if side == "b":
            return (x + w / 2 if off is None else off, y + h)
        if side == "l":
            return (x, y + h / 2 if off is None else off)
        return (x + w, y + h / 2 if off is None else off)

    def above(self, x1, x2, y, value, gap=6):
        return (x1 + x2) / 2, y - gap - lsize(lines_of(value))[1] / 2

    def below(self, x1, x2, y, value, gap=6):
        return (x1 + x2) / 2, y + gap + lsize(lines_of(value))[1] / 2

    # -- edges
    def keep(self, eid):
        """A source edge, unchanged (both ends must be in this view at their real positions)."""
        e = copy.deepcopy(S[eid])
        self.edges.append(e)

    def edge(self, eid, src, tgt, pts, value=None, at=None, color=None, sw=None):
        """A re-routed copy of source edge `eid` (label and colour kept) along explicit orthogonal points."""
        value = val(eid) if value is None else value
        color = color or sstyle(eid, "strokeColor") or BLACK
        sw = sw or float(sstyle(eid, "strokeWidth") or 1.5)
        for a, b in zip(pts, pts[1:]):
            assert a[0] == b[0] or a[1] == b[1], "non-orthogonal %s" % eid
        sx, sy, sw_, sh = self.reg[src]
        tx, ty, tw_, th = self.reg[tgt]
        style = ("edgeStyle=none;rounded=0;html=1;endArrow=open;endFill=0;endSize=10;strokeColor=%s;strokeWidth=%g;"
                 "fontSize=11;fontFamily=Helvetica;fontColor=%s;labelBackgroundColor=none;align=center;"
                 "exitX=%.4f;exitY=%.4f;exitDx=0;exitDy=0;exitPerimeter=0;"
                 "entryX=%.4f;entryY=%.4f;entryDx=0;entryDy=0;entryPerimeter=0;"
                 % (color, sw, color, (pts[0][0] - sx) / sw_, (pts[0][1] - sy) / sh,
                    (pts[-1][0] - tx) / tw_, (pts[-1][1] - ty) / th))
        e = ET.Element("mxCell", id=eid, value=value, style=style, edge="1", parent="1", source=src, target=tgt)
        g = ET.SubElement(e, "mxGeometry", relative="1")
        g.set("as", "geometry")
        if at is None:
            at = mid(pts)
        mx, my = mid(pts)
        off = ET.SubElement(g, "mxPoint", x="%.1f" % (at[0] - mx), y="%.1f" % (at[1] - my))
        off.set("as", "offset")
        if len(pts) > 2:
            arr = ET.SubElement(g, "Array")
            arr.set("as", "points")
            for p in pts[1:-1]:
                ET.SubElement(arr, "mxPoint", x="%g" % p[0], y="%g" % p[1])
        self.edges.append(e)

    # -- label centres beside a vertical line at x
    def right(self, x, y, value, gap=8):
        return x + gap + lsize(lines_of(value))[0] / 2, y

    def left(self, x, y, value, gap=8):
        return x - gap - lsize(lines_of(value))[0] / 2, y

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, self.name + ".drawio")
        mf = ET.Element("mxfile", host="app.diagrams.net", agent="amu-fsm-split")
        d = ET.SubElement(mf, "diagram", id=self.name, name=self.name)
        m = ET.SubElement(d, "mxGraphModel", dx="1400", dy="900", grid="1", gridSize="10", guides="1", tooltips="1",
                          connect="1", arrows="1", fold="1", page="1", pageScale="1", pageWidth="2400",
                          pageHeight="2400", math="0", shadow="0")
        root = ET.SubElement(m, "root")
        ET.SubElement(root, "mxCell", id="0")
        ET.SubElement(root, "mxCell", id="1", parent="0")
        for c in self.under + self.boxes + self.edges + self.over:
            root.append(c)
        ET.ElementTree(mf).write(path, encoding="utf-8", xml_declaration=False)
        return path


# ================================================================================ 01 overview (from scratch)
def overview():
    v = View("01_overview", "AMU parcel-run FSM: the big picture",
             "Phases only. Arrows are the main path; red arrows are faults. The next pictures zoom into each "
             "phase, one State per box, in the same notation.", 40, 20, 920)
    BOX = ("rounded=1;absoluteArcSize=1;arcSize=12;html=1;whiteSpace=wrap;verticalAlign=top;align=center;"
           "spacingTop=8;spacingLeft=8;spacingRight=8;fontSize=11;fontFamily=Helvetica;strokeWidth=1.5;")
    W, H, X = 330, 100, 260
    cx = X + W / 2
    ys = [150, 320, 490, 660, 830]
    spec = [
        ("o_idle", "IDLE", "waits for B0 (start button).<br>Leaving it resets the run.", "#ffffff", "#000000"),
        ("o_search", "SEARCH phase", "SEARCH · IDENTIFY<br><i>spin, find the bollard, learn Layout A / B</i>",
         "#eeeeee", "#666666"),
        ("o_orient", "ORIENT", "turn to the corridor: A -75°, B +108°<br><i>heading is saved</i>", "#ffffff", "#000000"),
        ("o_path", "PATHFIND", "DRIVE · REVERSE · ROTATE<br>CORRIDOR_REVERSE · CORRIDOR_TURN<br>CORRIDOR_STOP",
         "#eeeeee", "#666666"),
        ("o_store", "STORAGE", "TURN · DRIVE_IN · DEPOSIT<br>DEPOSIT_ALIGN · DEPOSIT_REFINE_*<br>"
                               "DEPOSIT_DISTANCE / _REVERSE / _APPROACH", "#eeeeee", "#666666"),
    ]
    for (cid, name, body, fill, stroke), y in zip(spec, ys):
        v._cell(v.boxes, cid, "<b style=\"font-size:14px\">%s</b><br><br>%s" % (name, body),
                BOX + "fillColor=%s;strokeColor=%s;" % (fill, stroke), X, y, W, H)
    v._cell(v.boxes, "o_start", "", "ellipse;html=1;fillColor=#000000;strokeColor=#000000;strokeWidth=1.5;",
            cx - 8, 106, 16, 16)
    fx, fw, fy = 790, 210, 320
    v._cell(v.boxes, "o_fault", "<b style=\"font-size:14px\">STOP / FAULT</b><br><b>entry /</b> "
            "<font face=\"Courier New\">Kobuki_Drive(STOP)</font>",
            BOX + "fillColor=#f8cecc;strokeColor=#b85450;", fx, fy, fw, ys[4] + H / 2 + 40 - fy)

    def mk(eid, s, t, pts, lines, color=BLACK, at=None, sw=1.5):
        v.edge(eid, s, t, pts, value="<br>".join(lines), at=at, color=color, sw=sw)

    mk("o_e0", "o_start", "o_idle", [(cx, 122), (cx, ys[0])], [])
    for i, lines in enumerate([["B0"], ["object found;", "IDENTIFY picks", "Layout A / B"], ["turn done;", "heading saved"],
                               ["2nd front cliff", "(corridor == 2)"]]):
        a, b = ys[i] + H, ys[i + 1]
        mk("o_e%d" % (i + 1), spec[i][0], spec[i + 1][0], [(cx, a), (cx, b)], lines, at=v.right(cx, (a + b) / 2, "<br>".join(lines)))
    ret = ["done: LED1 green + win sound", "(or no rod found: LED1 red)"]
    mk("o_e5", "o_store", "o_idle", [(X, ys[4] + H / 2), (X - 30, ys[4] + H / 2), (X - 30, ys[0] + H / 2), (X, ys[0] + H / 2)],
       ret, at=v.left(X - 30, 540, "<br>".join(ret)))
    for cid, y, lines in [("o_search", ys[1], ["B1 | wheeldrop", "| bumper | cliff"]),
                          ("o_orient", ys[2], ["B1 | wheeldrop", "| bumper | cliff"]),
                          ("o_path", ys[3], ["B1 | wheeldrop"]), ("o_store", ys[4], ["B1 | wheeldrop"])]:
        yy = y + H / 2
        mk("o_f_" + cid, cid, "o_fault", [(X + W, yy), (fx, yy)], lines, RED, at=v.above(X + W, fx, yy, "<br>".join(lines)), sw=1.25)
    yy = ys[0] + H / 2
    mk("o_e6", "o_fault", "o_idle", [(fx + fw / 2, fy), (fx + fw / 2, yy), (X + W, yy)], ["ε (next Tick)"],
       at=v.above(X + W, fx + fw / 2, yy, "ε (next Tick)"))
    v.note(40, ys[4] + H + 50, 960, 72,
           "<b>Detailed pictures</b>  02_search_orient: IDLE, SEARCH, IDENTIFY, ORIENT · 03_pathfind: DRIVE, REVERSE, ROTATE, "
           "CORRIDOR_* · 04_storage_entry_scan: TURN, DRIVE_IN, DEPOSIT · 05_storage_align_refine: DEPOSIT_ALIGN, "
           "DEPOSIT_REFINE_TURN / _SAMPLE · 06_storage_distance_adjust: DEPOSIT_DISTANCE / _REVERSE / _APPROACH.<br>"
           "<b>Faults</b>  B1 | wheeldrop leaves every State except IDLE and STOP / FAULT. STOP / FAULT stops the motors and "
           "returns to IDLE on the next Tick. PATHFIND and most of STORAGE expect bumpers and cliffs, so they fault on "
           "B1 | wheeldrop only; the three DISTANCE ADJUST States also fault on any bumper or cliff.")
    return v


# =========================================================================== 02 search / orient
def search_orient():
    v = View("02_search_orient", "1. Search and orient",
             "IDLE, SEARCH phase (SEARCH, IDENTIFY) and ORIENT. Dashed grey boxes are States shown in other pictures; "
             "red = fault. A / B is the Layout learned by IDENTIFY.", 40, 20, 860)
    X = 170                                   # main column x; boxes are centred on cx
    cx = X + 150
    v.ghost("g_dep", 120, 100, 190, 46, "DEPOSIT", "(STORAGE, picture 04)")
    v.ghost("g_dd", 330, 100, 220, 46, "DEPOSIT_DISTANCE", "(STORAGE, picture 06)")
    v.raw("start")
    v.reg["start"] = (110, 319, 16, 16)
    v.boxes[-1].find("mxGeometry").set("x", "110")
    v.boxes[-1].find("mxGeometry").set("y", "319")
    v.state("idle", (X, 250))                                   # 250..404
    v.composite("search_phase", "search_phase", 140, 464, 360, 358)   # 464..822
    v.state("search", (cx - 130, 522))                          # 522..602
    v.state("identify", (X, 672))                               # 672..792
    v.state("orient", (X, 892))                                 # 892..1012
    v.ghost("g_drive", X, 1092, 300, 46, "DRIVE", "(PATHFIND, picture 03)")
    v.fault_bar(680, 500, 220, 510)
    v.edge("e_start", "start", "idle", [(126, 327), (X, 327)])
    v.edge("e_deposit_idle", "g_dep", "idle", [(215, 146), (215, 250)], at=v.right(215, 198, val("e_deposit_idle")))
    v.edge("e_dist_idle", "g_dd", "idle", [(430, 146), (430, 250)], at=v.right(430, 198, val("e_dist_idle")))
    v.edge("e_idle_search", "idle", "search", [(235, 404), (235, 522)], at=v.right(235, 434, val("e_idle_search")))
    v.edge("e_search_identify", "search", "identify", [(cx, 602), (cx, 672)], at=v.right(cx, 637, val("e_search_identify")))
    v.edge("e_identify_orient", "identify", "orient", [(cx, 792), (cx, 892)], at=v.right(cx, 857, val("e_identify_orient")))
    v.edge("e_orient_drive", "orient", "g_drive", [(cx, 1012), (cx, 1092)], at=v.right(cx, 1052, val("e_orient_drive")))
    v.edge("f_search_giveup", "search", "fault", [(cx + 130, 562), (680, 562)],
           at=v.above(cx + 130, 680, 562, val("f_search_giveup")))
    v.edge("f_search_phase", "search_phase", "fault", [(500, 720), (680, 720)], at=v.above(500, 680, 720, val("f_search_phase")))
    v.edge("f_orient", "orient", "fault", [(X + 300, 952), (680, 952)], at=v.above(X + 300, 680, 952, val("f_orient")))
    v.edge("e_fault_idle", "fault", "idle", [(790, 500), (790, 327), (X + 300, 327)], at=v.above(X + 300, 790, 327, "ε"))
    v.note(60, 1170, 840, 62,
           "SEARCH turns anticlockwise on the spot from B0 and reads the ultrasonic once per Tick. A Detection is "
           "range_mm &lt; 750. IDENTIFY stops on the first Detection and sets the Layout: B when range_mm &lt; 575, "
           "otherwise A (LED1 red for A, yellow for B). There is no centring: ORIENT turns from wherever SEARCH stopped. "
           "A full turn without a bollard (scan_deg &gt;= 360) gives up through STOP / FAULT.")
    return v


# =================================================================================== 03 pathfind
def pathfind():
    v = View("03_pathfind", "2. Pathfind",
             "DRIVE, REVERSE, ROTATE and the CORRIDOR_ States. Dashed grey boxes are States shown in other pictures; "
             "red = fault.", 40, 20, 1090)
    LX, RX = 190, 610                                   # left (corridor) and right (recover) columns, 320 wide
    v.composite("pathfind", "pathfind", 40, 200, 1090, 652)
    v.state("p_drive", (RX, 270))                       # 270..350
    v.state("p_reverse", (RX, 500))                     # 500..594
    v.state("p_rotate", (RX, 674))                      # 674..798
    v.state("p_creverse", (LX, 400))                    # 400..494
    v.state("p_cturn", (LX, 574))                       # 574..698
    v.state("p_cstop", (110, 768))                      # 768..822 (230 wide)
    v.raw("p_choice")
    v.reg["p_choice"] = (336, 296, 28, 28)
    g = v.boxes[-1].find("mxGeometry")
    g.set("x", "336")
    g.set("y", "296")
    v.ghost("g_fault", LX, 96, 320, 46, "STOP / FAULT", "(picture 01)", fault=True)
    v.ghost("g_orient", RX, 96, 320, 46, "ORIENT", "(picture 02)")
    v.ghost("g_turn", 110, 900, 230, 46, "TURN", "(STORAGE, picture 04)")
    v.edge("f_pathfind", "pathfind", "g_fault", [(350, 200), (350, 142)], at=v.right(350, 171, val("f_pathfind")))
    v.edge("e_orient_drive", "g_orient", "p_drive", [(770, 142), (770, 270)], at=v.right(770, 171, val("e_orient_drive")))
    v.edge("p_drive_choice", "p_drive", "p_choice", [(RX, 310), (364, 310)], at=v.above(364, RX, 310, val("p_drive_choice")))
    v.edge("p_choice_creverse", "p_choice", "p_creverse", [(350, 324), (350, 400)], at=v.right(350, 362, val("p_choice_creverse")))
    v.edge("p_choice_stop", "p_choice", "p_cstop", [(336, 310), (140, 310), (140, 768)],
           at=v.above(140, 336, 310, val("p_choice_stop")))
    v.edge("p_creverse_cturn", "p_creverse", "p_cturn", [(350, 494), (350, 574)], at=v.right(350, 534, val("p_creverse_cturn")))
    v.edge("p_cturn_drive", "p_cturn", "p_drive", [(LX + 320, 636), (570, 636), (570, 335), (RX, 335)],
           at=v.right(570, 420, val("p_cturn_drive")))
    v.edge("p_drive_reverse", "p_drive", "p_reverse", [(770, 350), (770, 500)], at=v.right(770, 425, val("p_drive_reverse")))
    v.edge("p_reverse_rotate", "p_reverse", "p_rotate", [(770, 594), (770, 674)], at=v.right(770, 634, val("p_reverse_rotate")))
    v.edge("p_rotate_drive", "p_rotate", "p_drive", [(RX + 320, 736), (965, 736), (965, 310), (RX + 320, 310)],
           at=v.right(965, 520, val("p_rotate_drive")))
    v.edge("p_stop_turn", "p_cstop", "g_turn", [(225, 822), (225, 900)], at=v.right(225, 876, "ε"))
    v.note(40, 980, 1090, 50, S["pathfind_note"].get("value"))
    return v


# ================================================================================= 04 storage entry
def storage_entry():
    v = View("04_storage_entry_scan", "3a. Storage: drive in and scan for the rod",
             "TURN, DRIVE_IN and DEPOSIT (the full-circle scan). Dashed grey boxes are States shown in other pictures; "
             "red = fault.", 40, 20, 920)
    X = 100
    cx = X + 155
    v.ghost("p_cstop", X, 100, 310, 54, "CORRIDOR_STOP", "(PATHFIND, picture 03)")
    v.composite("storage", "storage", 40, 200, 600, 680)           # 200..880
    v.state("turn", (X, 258))                                       # 258..352
    v.state("drive_in", (X, 422))                                   # 422..516
    v.state("deposit", (X, 626))                                    # 626..734
    v.ghost("d_align", X, 804, 310, 46, "DEPOSIT_ALIGN", "(picture 05)")
    v.ghost("g_idle", 830, 657, 180, 46, "IDLE", "(picture 02)")
    v.ghost("g_fault", 830, 257, 180, 46, "STOP / FAULT", "(picture 01)", fault=True)
    v.edge("p_stop_turn", "p_cstop", "turn", [(cx, 154), (cx, 258)], at=v.right(cx, 177, "ε"))
    v.edge("e_turn_drivein", "turn", "drive_in", [(cx, 352), (cx, 422)], at=v.right(cx, 387, val("e_turn_drivein")))
    v.edge("e_drivein_deposit_bump", "drive_in", "deposit", [(150, 516), (150, 626)],
           at=v.right(150, 571, val("e_drivein_deposit_bump")))
    v.edge("e_drivein_deposit_done", "drive_in", "deposit", [(360, 516), (360, 626)],
           at=v.right(360, 571, val("e_drivein_deposit_done")))
    v.edge("e_deposit_align", "deposit", "d_align", [(cx, 734), (cx, 804)], at=v.right(cx, 769, val("e_deposit_align")))
    v.edge("e_deposit_idle", "deposit", "g_idle", [(X + 310, 680), (830, 680)], at=v.above(650, 830, 680, val("e_deposit_idle")))
    v.edge("f_storage", "storage", "g_fault", [(640, 280), (830, 280)], at=v.above(640, 830, 280, val("f_storage")))
    v.note(40, 910, 970, 50,
           "DEPOSIT turns anticlockwise through a full circle (scan_deg &gt;= 360) at 50 mm/s and keeps the closest valid echo "
           "(not 0, at most DEPOSIT_SCAN_MAX_RANGE_MM) with its gyro heading. With no echo it lights LED1 red and returns to "
           "IDLE. TURN ignores bumper and cliff; DRIVE_IN ignores the cliff, and any bumper ends it in DEPOSIT.")
    return v


# ============================================================================== 05 storage align/refine
def storage_refine():
    v = View("05_storage_align_refine", "3b. Storage: align to the rod and refine the heading",
             "DEPOSIT_ALIGN and the refine sweep (DEPOSIT_REFINE_TURN / DEPOSIT_REFINE_SAMPLE). Dashed grey boxes are States "
             "shown in other pictures; red = fault.", 40, 20, 1030)
    LX, RX = 80, 640
    v.ghost("g_fault", 640, 96, 400, 46, "STOP / FAULT", "(picture 01)", fault=True)
    v.composite("storage", "storage", 40, 200, 1030, 533)           # 200..733
    v.ghost_of("deposit", "(picture 04)", at=(LX, 258), h=46)       # 258..304
    v.state("d_align", (LX, 384))                                   # 384..492
    v.ghost_of("d_dist", "(DISTANCE ADJUST, picture 06)", at=(LX, 582), h=46)   # 582..628
    v.state("d_rturn", (RX, 367))                                   # 367..461
    v.state("d_rsample", (RX, 581))                                 # 581..703
    v.edge("f_storage", "storage", "g_fault", [(700, 200), (700, 142)], at=v.right(700, 171, val("f_storage")))
    v.edge("e_deposit_align", "deposit", "d_align", [(235, 304), (235, 384)], at=v.right(235, 344, val("e_deposit_align")))
    v.edge("e_align_dist", "d_align", "d_dist", [(235, 492), (235, 582)], at=v.right(235, 537, val("e_align_dist")))
    v.edge("e_align_rturn", "d_align", "d_rturn", [(LX + 310, 414), (RX, 414)], at=v.above(LX + 310, RX, 414, val("e_align_rturn")))
    v.edge("e_rturn_rsample", "d_rturn", "d_rsample", [(910, 461), (910, 581)], at=v.right(910, 521, val("e_rturn_rsample")))
    v.edge("e_rsample_rturn", "d_rsample", "d_rturn", [(700, 581), (700, 461)], at=v.right(700, 521, val("e_rsample_rturn")))
    v.edge("e_rsample_align", "d_rsample", "d_align", [(RX, 642), (475, 642), (475, 469), (LX + 310, 469)],
           at=v.right(475, 555, val("e_rsample_align")))
    v.edge("f_sweep", "d_rsample", "g_fault", [(RX + 310, 670), (1020, 670), (1020, 142)], at=v.left(1020, 290, val("f_sweep")))
    v.note(40, 760, 1030, 58,
           "DEPOSIT_ALIGN turns the short way to the saved heading. The first time, within 100 ticks, it starts the refine "
           "sweep; once the heading is refined, within 50 ticks, it goes to DEPOSIT_DISTANCE. The sweep visits 26 headings from "
           "-25 to +25 deg around the coarse heading, 2 deg apart. DEPOSIT_REFINE_TURN turns to a step at 35 mm/s; "
           "DEPOSIT_REFINE_SAMPLE stops, waits 15 Ticks, takes 3 readings 5 Ticks apart and keeps the median. After the last "
           "step, the centre of the widest run of steps within 10 mm of the closest median becomes the heading (ties: nearest "
           "the coarse heading). No valid median faults with LED1 red.")
    return v


# ============================================================================ 06 storage distance adjust
def storage_distance():
    v = View("06_storage_distance_adjust", "3c. Storage: adjust the distance to the rod",
             "DEPOSIT_DISTANCE, DEPOSIT_REVERSE and DEPOSIT_APPROACH. Dashed grey boxes are States shown in other pictures; "
             "red = fault.", 40, 20, 950)
    LX, RX = 100, 620
    v.ghost("g_fault", 400, 96, 560, 46, "STOP / FAULT", "(picture 01)", fault=True)
    v.composite("storage", "storage", 40, 200, 950, 540)           # 200..740
    v.ghost_of("d_align", "(picture 05)", at=(LX, 258), h=46)      # 258..304
    v.composite("dist_group", "dist_group", 70, 370, 890, 340)     # 370..710
    v.state("d_app", (RX, 428))                                    # 428..508
    v.state("d_dist", (LX, 520))                                   # 520..642
    v.state("d_rev", (RX, 600))                                    # 600..680
    v.ghost("g_idle", LX, 810, 310, 46, "IDLE", "(picture 02)")
    v.edge("f_dist_any", "dist_group", "g_fault", [(440, 370), (440, 142)], at=v.right(440, 173, val("f_dist_any")))
    v.edge("f_dist_range", "dist_group", "g_fault", [(700, 370), (700, 142)], at=v.right(700, 173, val("f_dist_range")))
    v.edge("f_storage", "storage", "g_fault", [(900, 200), (900, 142)], at=v.right(900, 173, val("f_storage")))
    v.edge("e_align_dist", "d_align", "d_dist", [(200, 304), (200, 520)], at=v.right(200, 337, val("e_align_dist")))
    v.edge("e_dist_app", "d_dist", "d_app", [(330, 520), (330, 455), (RX, 455)], at=v.above(330, RX, 455, val("e_dist_app")))
    v.edge("e_app_dist", "d_app", "d_dist", [(RX, 490), (380, 490), (380, 520)], at=v.below(380, RX, 490, val("e_app_dist")))
    v.edge("e_dist_rev", "d_dist", "d_rev", [(LX + 310, 620), (RX, 620)], at=v.above(LX + 310, RX, 620, val("e_dist_rev")))
    v.edge("e_rev_dist", "d_rev", "d_dist", [(RX, 638), (LX + 310, 638)], at=v.below(LX + 310, RX, 638, val("e_rev_dist")))
    v.edge("e_dist_idle", "d_dist", "g_idle", [(255, 642), (255, 810)], at=v.right(255, 676, val("e_dist_idle")))
    v.note(40, 880, 950, 58,
           "DEPOSIT_DISTANCE holds still for 15 Ticks (about 300 ms) so the 10 Hz sonar catches up, then reads deposit_range_mm "
           "against the target 410 mm +/- 10 mm: too close backs off in DEPOSIT_REVERSE until 510 mm (a 100 mm margin) at "
           "50 mm/s, too far creeps in DEPOSIT_APPROACH at 35 mm/s until 420 mm, so the last correction is always made moving "
           "forward. Inside the band it lights LED1 green, starts the win sound and returns to IDLE. All three States also fault "
           "on any bumper or cliff, or on a zero or maximum-range reading.")
    return v


def main():
    views = [overview(), search_orient(), pathfind(), storage_entry(), storage_refine(), storage_distance()]
    for v in views:
        path = v.write()
        print("wrote", path)
        if RENDER_PNG:
            ext = ".png" if "--png" in sys.argv else ".svg"
            img = path[:-len(".drawio")] + ext
            subprocess.run(["node", RENDER, path, img, "2"], check=True)
            if "--keep-drawio" not in sys.argv:
                os.remove(path)


if __name__ == "__main__":
    main()
