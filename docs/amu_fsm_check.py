#!/usr/bin/env python3
"""Check docs/amu_fsm.drawio against Src/main.c, Src/Helper/helper.h and Src/Ultrasonic/ultrasonic.h.

Usage:  python3 docs/amu_fsm_check.py [diagram.drawio]   (exit 1 on any mismatch)

It parses the State switch in main.c and compares every transition, which
States can fault (and with Fault_Any or Fault_B1_Wheeldrop, or with a
State-specific fault edge), every Kobuki_* / Scan_* / Deposit_* call a State
makes, and every constant in helper.h / ultrasonic.h with what the diagram
draws and says. Composite membership is read from the diagram's own parent
links, so nested composites (STORAGE > DISTANCE ADJUST) need no extra table.
Standard library only.
"""
import html
import os
import re
import sys
import xml.etree.ElementTree as ET

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRAWIO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "docs", "amu_fsm.drawio")

# ST_* enum name -> diagram cell id
ID = {
    "STATE_IDLE": "idle", "STATE_SEARCH": "search", "STATE_IDENTIFY": "identify", "STATE_ORIENT": "orient",
    "STATE_DRIVE": "p_drive", "STATE_REVERSE": "p_reverse", "STATE_ROTATE": "p_rotate",
    "STATE_CORRIDOR_REVERSE": "p_creverse", "STATE_CORRIDOR_TURN": "p_cturn", "STATE_CORRIDOR_STOP": "p_cstop",
    "STATE_TURN": "turn", "STATE_DRIVE_IN": "drive_in", "STATE_DEPOSIT": "deposit",
    "STATE_DEPOSIT_ALIGN": "d_align", "STATE_DEPOSIT_REFINE_TURN": "d_rturn",
    "STATE_DEPOSIT_REFINE_SAMPLE": "d_rsample", "STATE_DEPOSIT_DISTANCE": "d_dist",
    "STATE_DEPOSIT_REVERSE": "d_rev", "STATE_DEPOSIT_APPROACH": "d_app", "STATE_FAULT": "fault",
}
problems = []


def check(cond, msg):
    print(("PASS  " if cond else "FAIL  ") + msg)
    if not cond:
        problems.append(msg)


def plain_text(v):
    v = re.sub(r"<br\s*/?>", " ", v or "")
    v = re.sub(r"<[^>]+>", " ", v.replace("&nbsp;", " "))   # strip tags per cell, BEFORE unescaping
    return re.sub(r"\s+", " ", html.unescape(v))


# ------------------------------------------------------------ code side
main_c = open(REPO + "/Src/main.c").read()
loop = main_c[main_c.index("switch (state) {", main_c.index("int main")):]
blocks = re.split(r"\n\s+case (ST_\w+):", loop)
code_edges, code_pred, code_specific, code_body = set(), {}, {}, {}
for name, body in zip(blocks[1::2], blocks[2::2]):
    src = ID[name]
    code_body[src] = body
    for m in re.finditer(r"change_to\(([^;]*)\);", body):
        for t in re.findall(r"\bST_\w+", m.group(1)):
            code_edges.add((src, ID[t]))
    code_pred[src] = ("Fault_Any" if "Fault_Any()" in body
                      else "Fault_B1_Wheeldrop" if "Fault_B1_Wheeldrop()" in body else None)
    # a fault edge that is not just the Fault_*() predicate check is State-specific (give-up, no echo ...)
    rest = re.sub(r"if \(Fault_\w+\(\)\) \{\s*change_to\(STATE_FAULT\);\s*break;\s*\}", "", body)
    code_specific[src] = "change_to(STATE_FAULT)" in rest

states_enum = set(re.findall(r"\bST_\w+", main_c[:main_c.index("} State;")]))
check(states_enum == set(ID), "ST_* enum == the %d States the diagram maps (%s)" % (len(ID), sorted(states_enum ^ set(ID))))
check(len(blocks[1::2]) == len(ID), "main.c switch has %d State cases (found %d)" % (len(ID), len(blocks[1::2])))

# ---------------------------------------------------------- diagram side
tree = ET.parse(DRAWIO)
cells = {c.get("id"): c for c in tree.iter("mxCell")}
STATE_IDS = set(ID.values())


def ancestors(cid):
    out = []
    while cid in cells:
        p = cells[cid].get("parent")
        if p is None or p in ("0", "1") or p not in cells:
            break
        out.append(p)
        cid = p
    return out


members = {}                                   # composite id -> the State ids inside it, at any depth
for sid in STATE_IDS:
    for a in ancestors(sid):
        members.setdefault(a, set()).add(sid)
check(set(members) >= {"search_phase", "pathfind", "storage", "dist_group"},
      "composites found in the diagram: %s" % sorted(members))


def expand(cid):
    if cid == "p_choice":
        return {"p_drive"}                      # the choice is a pseudostate of DRIVE's front-cliff edge
    return members.get(cid, {cid})


dia_edges, dia_pred, dia_specific = set(), {}, set()
raw = [(c.get("source"), c.get("target"), c.get("id"), plain_text(c.get("value"))) for c in cells.values()
       if c.get("edge") == "1"]
for s, t, eid, label in raw:
    if s == "start" or t == "p_choice":
        continue
    for x in expand(s):
        if t == "fault":
            if "B1 | wheeldrop" in label:
                kind = "Fault_Any" if "bumper" in label else "Fault_B1_Wheeldrop"
                if dia_pred.get(x) != "Fault_Any":
                    dia_pred[x] = kind
            else:
                dia_specific.add(x)
        else:
            dia_edges.add((x, t))

code_nonfault = {e for e in code_edges if e[1] != "fault"}
check(code_nonfault == dia_edges, "non-fault transitions identical: code %d, diagram %d" %
      (len(code_nonfault), len(dia_edges)))
for e in sorted(code_nonfault - dia_edges):
    print("   in code, not in diagram:", e)
for e in sorted(dia_edges - code_nonfault):
    print("   in diagram, not in code:", e)

want_pred = {s: k for s, k in code_pred.items() if k}
check(dia_pred == want_pred, "Fault_Any / Fault_B1_Wheeldrop edges match per State (%d States)" % len(want_pred))
for s in sorted(set(dia_pred) | set(want_pred)):
    if dia_pred.get(s) != want_pred.get(s):
        print("   %s: code %s, diagram %s" % (s, want_pred.get(s), dia_pred.get(s)))
want_specific = {s for s, k in code_specific.items() if k}
check(dia_specific == want_specific, "State-specific fault edges match: %s" % sorted(want_specific))
check(code_pred["idle"] is None and code_pred["fault"] is None and not code_specific["idle"],
      "IDLE and FAULT have no fault edge in code")
check({"idle", "fault"}.isdisjoint(dia_pred) and {"idle", "fault"}.isdisjoint(dia_specific),
      "IDLE and FAULT have no fault edge in the diagram")

# ------------------------------------------------------------- constants
helper_h = open(REPO + "/Src/Helper/helper.h").read()


def define(name):
    m = re.search(r"#define\s+%s\s+\(?(-?(?:0x[0-9A-Fa-f]+|\d+))U?\)?" % name, helper_h)
    return int(m.group(1), 0)


text = re.sub(r"\s+", " ", " ".join(plain_text(c.get("value")) for c in cells.values()))


def sgn(n):
    return ("+%d" % n) if n > 0 else str(n)


consts = {
    "SCAN_ROTATE_SPEED_MM_S": ("Kobuki_Rotate(%d)", define("SCAN_ROTATE_SPEED_MM_S")),
    "SCAN_FULL_TURN_DEG": ("scan_deg >= %d", define("SCAN_FULL_TURN_DEG")),
    "LAYOUT_B_MAX_RANGE_MM": ("range_mm < %d", define("LAYOUT_B_MAX_RANGE_MM")),
    "PATHFIND_DRIVE_SPEED_MM_S": ("Kobuki_Drive(%d)", define("PATHFIND_DRIVE_SPEED_MM_S")),
    "STORAGE_DRIVE_IN_MM": ("kobuki.distance, %d)", define("STORAGE_DRIVE_IN_MM")),
    "PATHFIND_REVERSE_MM": ("kobuki.distance, -%d)", define("PATHFIND_REVERSE_MM")),
    "DEPOSIT_SCAN_SPEED_MM_S": ("Kobuki_Rotate(%d)", define("DEPOSIT_SCAN_SPEED_MM_S")),
    "DEPOSIT_REFINE_SPEED_MM_S": ("refined: %d mm/s", define("DEPOSIT_REFINE_SPEED_MM_S")),
    "DEPOSIT_REFINE_TOLERANCE_TICKS": ("|turn| <= %d", define("DEPOSIT_REFINE_TOLERANCE_TICKS")),
    "DEPOSIT_ALIGN_TOLERANCE_TICKS": ("|turn| <= %d", define("DEPOSIT_ALIGN_TOLERANCE_TICKS")),
    "DEPOSIT_REFINE_HALF_ANGLE_DEG": ("-%d to +%d deg", define("DEPOSIT_REFINE_HALF_ANGLE_DEG")),
    "DEPOSIT_REFINE_STEP_DEG": ("%d deg apart", define("DEPOSIT_REFINE_STEP_DEG")),
    "DEPOSIT_REFINE_RANGE_BAND_MM": ("within %d mm", define("DEPOSIT_REFINE_RANGE_BAND_MM")),
    "DEPOSIT_REFINE_SAMPLE_PACKETS": ("%d Ticks apart", define("DEPOSIT_REFINE_SAMPLE_PACKETS")),
    "DEPOSIT_SETTLE_PACKETS": ("deposit_settle_packets = %d", define("DEPOSIT_SETTLE_PACKETS")),
    "DEPOSIT_REVERSE_SPEED_MM_S": ("Kobuki_Drive(-%d)", define("DEPOSIT_REVERSE_SPEED_MM_S")),
    "DEPOSIT_APPROACH_SPEED_MM_S": ("Kobuki_Drive(%d)", define("DEPOSIT_APPROACH_SPEED_MM_S")),
}
for k, (fmt, v) in consts.items():
    want = fmt % ((v, v) if fmt.count("%d") == 2 else v)
    check(want in text, "%s = %d appears as '%s'" % (k, v, want))
# the two Deposit tolerances are tied to the speeds in the ALIGN box: "coarse: 50 mm/s, 100 ticks"
cs, ct = define("DEPOSIT_SCAN_SPEED_MM_S"), define("DEPOSIT_ALIGN_TOLERANCE_TICKS")
rs, rt = define("DEPOSIT_REFINE_SPEED_MM_S"), define("DEPOSIT_REFINE_TOLERANCE_TICKS")
check("coarse: %d mm/s, %d ticks" % (cs, ct) in text and "refined: %d mm/s, %d ticks" % (rs, rt) in text,
      "ALIGN speeds and tolerances: coarse %d / %d, refined %d / %d" % (cs, ct, rs, rt))
points = 2 * define("DEPOSIT_REFINE_HALF_ANGLE_DEG") // define("DEPOSIT_REFINE_STEP_DEG") + 1   # DEPOSIT_REFINE_POINTS
check("%d steps" % points in text, "refine sweep has %d steps" % points)

tgt, tol = define("DEPOSIT_TARGET_RANGE_MM"), define("DEPOSIT_RANGE_TOLERANCE_MM")
margin = define("DEPOSIT_REVERSE_MARGIN_MM")
for fmt, val in (("deposit_range_mm < %d", tgt - tol), ("deposit_range_mm > %d", tgt + tol),
                 ("deposit_range_mm >= %d", tgt + margin), ("deposit_range_mm <= %d", tgt + tol),
                 ("%d <= deposit_range_mm", tgt - tol)):
    check(fmt % val in text, "distance band '%s'" % (fmt % val))
check("%d mm" % tgt in text and "+/- %d mm" % tol in text and "%d mm" % margin in text,
      "target %d mm +/- %d mm, reverse margin %d mm are stated" % (tgt, tol, margin))

for a, b in [("ORIENT_TURN_LAYOUT_A_DEG", "ORIENT_TURN_LAYOUT_B_DEG"),
             ("PATHFIND_CORNER_TURN_LAYOUT_A_DEG", "PATHFIND_CORNER_TURN_LAYOUT_B_DEG"),
             ("STORAGE_TURN_LAYOUT_A_DEG", "STORAGE_TURN_LAYOUT_B_DEG")]:
    s = "A: %s, B: %s)" % (sgn(define(a)), sgn(define(b)))
    check(s in text, "%s / %s appear as '%s'" % (a, b, s))
ra, rb = define("PATHFIND_RECOVER_TURN_LAYOUT_A_DEG"), define("PATHFIND_RECOVER_TURN_LAYOUT_B_DEG")
check("wall A %d, B %s" % (ra, sgn(rb)) in text and "recover = A %d | B %s" % (ra, sgn(rb)) in text,
      "wall-hit nudge A %d / B %s" % (ra, sgn(rb)))
check("ledge A %s, B %d" % (sgn(-ra), -rb) in text and "recover = A %s | B %d" % (sgn(-ra), -rb) in text,
      "ledge nudge is the opposite: A %s / B %d" % (sgn(-ra), -rb))
ultra = open(REPO + "/Src/Ultrasonic/ultrasonic.h").read()
detect = re.search(r"ULTRASONIC_DETECT_RANGE_MM \(\(uint16_t\)(\d+)U\)", ultra).group(1)
maxr = re.search(r"ULTRASONIC_MAX_RANGE_MM\s+\(\(uint16_t\)(\d+)U\)", ultra).group(1)
check("range_mm < %s" % detect in text, "detect range %s appears" % detect)
check("%s mm" % maxr in text, "maximum range %s mm appears (no echo)" % maxr)
check(re.search(r"#define DEPOSIT_TEST_MODE 0", helper_h) is not None and "DEPOSIT_TEST_MODE" in text,
      "DEPOSIT_TEST_MODE (default 0) is drawn")
b0, b1 = define("KOBUKI_BUTTON_B0_MASK"), define("KOBUKI_BUTTON_B1_MASK")
check(b0 == 1 and "kobuki.button & 0x01" in text, "B0 = button & 0x01")
check(b1 == 2 and "kobuki.button & 0x02" in text, "B1 = button & 0x02")
right, centre, left = (define("KOBUKI_SIDE_%s_MASK" % n) for n in ("RIGHT", "CENTRE", "LEFT"))
check((right, centre, left) == (1, 2, 4) and "right = 0x01, centre = 0x02, left = 0x04" in text,
      "side masks right 0x01, centre 0x02, left 0x04")
check(centre == 2 and "kobuki.cliff & 0x02" in text, "front cliff = cliff & 0x02")
check(("kobuki.bumper & 0x%02x" % left) in text and ("kobuki.bumper & 0x%02x" % right) in text,
      "wall bumper masks (A left 0x04, B right 0x01)")
check(("kobuki.cliff & 0x%02x" % right) in text and ("kobuki.cliff & 0x%02x" % left) in text,
      "ledge cliff masks (A right 0x01, B left 0x04)")


# ---------------------------------------------- per-State entry / do actions
# Every Kobuki_* / Scan_* / Deposit_* call that main.c makes for a State (entry in change_to, or anywhere
# in its switch case) must be written in that State's box, or in the label of an edge that leaves the State
# or a composite around it (an edge effect such as "/ Kobuki_Drive(STOP)"). LED calls may be written "LED1".
def body_of(cid):
    return plain_text(cells[cid + "_body"].get("value")) if cid + "_body" in cells else ""


def label_text(cid):
    srcs = {cid} | set(ancestors(cid))
    if cid == "p_drive":
        srcs.add("p_choice")
    return " ".join(lbl for s, t, eid, lbl in raw if s in srcs)


IGNORED = {"Scan_Gave_Up", "Kobuki_IsCliffDetected"}      # predicates, drawn as guards, not as actions


def shown(fn, cid):
    where = body_of(cid) + " " + label_text(cid)
    return fn in where or (fn == "Kobuki_Update_LEDs" and "LED1" in where)


entry_src = main_c[main_c.index("state = next;"):main_c.index("// Initialise the peripherals")]
for m in re.finditer(r"((?:\s*case ST_\w+:)+)(.*?)break;", entry_src, re.S):
    calls = [f for f in re.findall(r"\b((?:Kobuki|Scan|Deposit)_\w+)\(", m.group(2)) if f not in IGNORED]
    for name in re.findall(r"\bST_\w+", m.group(1)):
        for fn in dict.fromkeys(calls):
            check("entry / " in body_of(ID[name]) and fn in body_of(ID[name]),
                  "%s entry calls %s() in its box" % (name, fn))
for name, body in zip(blocks[1::2], blocks[2::2]):
    for fn in dict.fromkeys(f for f in re.findall(r"\b((?:Kobuki|Scan|Deposit)_\w+)\(", body) if f not in IGNORED):
        check(shown(fn, ID[name]), "%s calls %s() in its box or edge labels" % (name, fn))

# stale names must be gone
for stale in ["scan_sample()", "scan_give_up()", "Kobuki_Drive(100)", "Rotate(+100)", "A: -90", "B: +135", "1000 mm",
              "Kobuki_Drive(-100)", "SCAN_SPEED_MM_S", "SCAN_FULL_DEG", "LAYOUT_B_MAX_MM", "PATHFIND_ROTATE_",
              "PATHFIND_CORNER_A", "ORIENT_A_DEG", "stub", "not built yet", "(stub"]:
    check(stale not in text, "stale text gone: %r" % stale)

print()
print("RESULT:", "ALL CHECKS PASSED" if not problems else "%d FAILED" % len(problems))
sys.exit(1 if problems else 0)
