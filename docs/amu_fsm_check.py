#!/usr/bin/env python3
"""Check docs/amu_fsm.drawio against Src/main.c and Src/Helper/helper.h.

Usage:  python3 docs/amu_fsm_check.py [diagram.drawio]   (exit 1 on any mismatch)

It parses the State switch in main.c and compares every transition, which
States can fault (and with Fault_Any or Fault_B1_Wheeldrop), and every
constant in helper.h / ultrasonic.h with what the diagram draws and says.
Standard library only.
"""
import html
import os
import re
import sys
import xml.etree.ElementTree as ET

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRAWIO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "docs", "amu_fsm.drawio")

ID = {
    "ST_IDLE": "idle", "ST_SEARCH": "search", "ST_IDENTIFY": "identify", "ST_ORIENT": "orient",
    "ST_DRIVE": "p_drive", "ST_REVERSE": "p_reverse", "ST_ROTATE": "p_rotate",
    "ST_CORRIDOR_REVERSE": "p_creverse", "ST_CORRIDOR_TURN": "p_cturn", "ST_CORRIDOR_STOP": "p_cstop",
    "ST_TURN": "turn", "ST_DRIVE_IN": "drive_in", "ST_DEPOSIT": "deposit", "ST_FAULT": "fault",
}
COMPOSITE = {
    "search_phase": ["search", "identify"],
    "pathfind": ["p_drive", "p_reverse", "p_rotate", "p_creverse", "p_cturn", "p_cstop"],
    "storage": ["turn", "drive_in", "deposit"],
}
problems = []


def check(cond, msg):
    print(("PASS  " if cond else "FAIL  ") + msg)
    if not cond:
        problems.append(msg)


# ------------------------------------------------------------ code side
main_c = open(REPO + "/Src/main.c").read()
loop = main_c[main_c.index("switch (state) {", main_c.index("int main")):]
blocks = re.split(r"\n\s+case (ST_\w+):", loop)
code_edges, code_fault = set(), {}
for name, body in zip(blocks[1::2], blocks[2::2]):
    src = ID[name]
    for m in re.finditer(r"change_to\(([^;]*)\);", body):
        arg = m.group(1)
        for t in re.findall(r"ST_\w+", arg):
            code_edges.add((src, ID[t]))
    code_fault[src] = ("Fault_Any" if "Fault_Any()" in body
                       else "Fault_B1_Wheeldrop" if "Fault_B1_Wheeldrop()" in body else None)
check(len(blocks[1::2]) == 14, "main.c switch has 14 State cases (found %d)" % len(blocks[1::2]))
states_enum = set(re.findall(r"ST_\w+", main_c[:main_c.index("} State;")]))
check(states_enum == set(ID), "ST_* enum == the 14 States the diagram maps (%s)" % sorted(states_enum ^ set(ID)))

# ---------------------------------------------------------- diagram side
tree = ET.parse(DRAWIO)
cells = {c.get("id"): c for c in tree.iter("mxCell")}
raw_edges = [(c.get("source"), c.get("target"), c.get("id")) for c in cells.values() if c.get("edge") == "1"]
dia_edges, dia_fault = set(), {}
for s, t, eid in raw_edges:
    if s == "start":
        continue
    srcs = COMPOSITE.get(s, [s])
    if t == "fault" and s in COMPOSITE or (t == "fault" and s in ("orient", "search")):
        kind = "Fault_B1_Wheeldrop" if s in ("pathfind", "storage") else "Fault_Any"
        for x in srcs:
            dia_fault[x] = kind
    if s == "p_choice":
        s = "p_drive"          # the choice is a pseudostate of DRIVE's front-cliff edge
    if t == "p_choice":
        continue
    for x in COMPOSITE.get(s, [s]):
        dia_edges.add((x, t))
dia_edges = {e for e in dia_edges if not (e[1] == "fault" and e[0] in dia_fault and
                                          (e[0] in sum(COMPOSITE.values(), []) or e[0] == "orient")
                                          and not any(r[2] == "f_search_giveup" and e[0] == "search" for r in raw_edges)
                                          )} | {("search", "fault")}

code_nonfault = {e for e in code_edges if e[1] != "fault"}
dia_nonfault = {e for e in dia_edges if e[1] != "fault"}
check(code_nonfault == dia_nonfault, "non-fault transitions identical: code %d, diagram %d" %
      (len(code_nonfault), len(dia_nonfault)))
for e in sorted(code_nonfault - dia_nonfault):
    print("   in code, not in diagram:", e)
for e in sorted(dia_nonfault - code_nonfault):
    print("   in diagram, not in code:", e)

# fault predicate per State, and which States can reach FAULT at all
code_reach_fault = {s for s, k in code_fault.items() if k} | {"search"}
check({s for s, k in dia_fault.items()} == {s for s, k in code_fault.items() if k},
      "States with a fault edge match (code: %s)" % sorted(s for s, k in code_fault.items() if k))
check(all(dia_fault.get(s) == k for s, k in code_fault.items() if k),
      "Fault_Any vs Fault_B1_Wheeldrop matches per State")
check(code_fault["idle"] is None and code_fault["fault"] is None, "IDLE and FAULT have no fault edge in code or diagram")
check(("search", "fault") in code_edges, "SEARCH give-up (-> FAULT) exists in code")
check(any(r[2] == "f_search_giveup" for r in raw_edges), "SEARCH give-up edge drawn")

# ------------------------------------------------------------- constants
helper_h = open(REPO + "/Src/Helper/helper.h").read()


def define(name):
    m = re.search(r"#define\s+%s\s+\(?(-?(?:0x[0-9A-Fa-f]+|\d+))U?\)?" % name, helper_h)
    return int(m.group(1), 0)


def plain_text(v):
    v = re.sub(r"<[^>]+>", " ", v.replace("&nbsp;", " "))   # strip tags per cell, BEFORE unescaping
    return html.unescape(v)


text = " ".join(plain_text(c.get("value") or "") for c in cells.values())
text = re.sub(r"\s+", " ", text)


def sgn(n):
    return ("+%d" % n) if n > 0 else str(n)


consts = {
    "SCAN_ROTATE_SPEED_MM_S": ("Kobuki_Rotate(%d)", define("SCAN_ROTATE_SPEED_MM_S")),
    "SCAN_FULL_TURN_DEG": ("scan_deg >= %d", define("SCAN_FULL_TURN_DEG")),
    "LAYOUT_B_MAX_RANGE_MM": ("range_mm < %d", define("LAYOUT_B_MAX_RANGE_MM")),
    "PATHFIND_DRIVE_SPEED_MM_S": ("Kobuki_Drive(%d)", define("PATHFIND_DRIVE_SPEED_MM_S")),
    "STORAGE_DRIVE_IN_MM": ("kobuki.distance, %d)", define("STORAGE_DRIVE_IN_MM")),
    "PATHFIND_REVERSE_MM": ("kobuki.distance, -%d)", define("PATHFIND_REVERSE_MM")),
}
for k, (fmt, v) in consts.items():
    check(fmt % v in text, "%s = %d appears as '%s'" % (k, v, fmt % v))
pairs = [("ORIENT_TURN_LAYOUT_A_DEG", "ORIENT_TURN_LAYOUT_B_DEG"),
         ("PATHFIND_CORNER_TURN_LAYOUT_A_DEG", "PATHFIND_CORNER_TURN_LAYOUT_B_DEG"),
         ("STORAGE_TURN_LAYOUT_A_DEG", "STORAGE_TURN_LAYOUT_B_DEG")]
for a, b in pairs:
    s = "A: %s, B: %s)" % (sgn(define(a)), sgn(define(b)))
    check(s in text, "%s / %s appear as '%s'" % (a, b, s))
ra, rb = define("PATHFIND_RECOVER_TURN_LAYOUT_A_DEG"), define("PATHFIND_RECOVER_TURN_LAYOUT_B_DEG")
check("wall A %d, B %s" % (ra, sgn(rb)) in text and "recover = A %d | B %s" % (ra, sgn(rb)) in text,
      "wall-hit nudge A %d / B %s" % (ra, sgn(rb)))
check("ledge A %s, B %d" % (sgn(-ra), -rb) in text and "recover = A %s | B %d" % (sgn(-ra), -rb) in text,
      "ledge nudge is the opposite: A %s / B %d" % (sgn(-ra), -rb))
ultra = open(REPO + "/Src/Ultrasonic/ultrasonic.h").read()
detect = re.search(r"ULTRASONIC_DETECT_RANGE_MM \(\(uint16_t\)(\d+)U\)", ultra).group(1)
check("range_mm < %s" % detect in text, "detect range %s appears" % detect)
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
# Every Kobuki_* / Scan_* call that main.c runs as a State's entry (change_to) or as its
# top-level do (the step that runs when no edge fired) must be written in that State's box.
def body_of(cid):
    return re.sub(r"\s+", " ", plain_text(cells[cid + "_body"].get("value") or ""))


entry_src = main_c[main_c.index("state = next;"):main_c.index("// Initialise the peripherals")]
for m in re.finditer(r"((?:\s*case ST_\w+:)+)(.*?)break;", entry_src, re.S):
    calls = re.findall(r"\b((?:Kobuki|Scan)_\w+)\(", m.group(2))
    for name in re.findall(r"ST_\w+", m.group(1)):
        for fn in calls:
            check("entry / " in body_of(ID[name]) and fn in body_of(ID[name]),
                  "%s entry calls %s() in its box" % (name, fn))
for name, body in zip(blocks[1::2], blocks[2::2]):
    for fn in re.findall(r"^ {16}((?:Kobuki|Scan)_\w+)\(", body, re.M):
        check(fn in body_of(ID[name]), "%s do calls %s() in its box" % (name, fn))

# stale names must be gone
for stale in ["scan_sample()", "scan_give_up()", "Kobuki_Drive(100)", "Rotate(+100)", "A: -90", "B: +135", "1000 mm", "-100",
              "SCAN_SPEED_MM_S", "SCAN_FULL_DEG", "LAYOUT_B_MAX_MM", "PATHFIND_ROTATE_", "PATHFIND_CORNER_A", "ORIENT_A_DEG"]:
    check(stale not in text, "stale text gone: %r" % stale)

print()
print("RESULT:", "ALL CHECKS PASSED" if not problems else "%d FAILED" % len(problems))
sys.exit(1 if problems else 0)
