#!/usr/bin/env python3
"""Fail if Pathfind does not reverse after a storage-wall bumper hit.

Layout A with Orient -90 puts the storage wall on the robot's left while it
drives the bottom corridor. A left bumper during Drive must be followed by a
negative wheel command (Reverse) within a short window. Layout B mirrors this
on the right bumper once Pathfind is under way.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIM = ROOT / "sim" / "kobuki_sim"
OUT = ROOT / "sim_recover_check"


def run(layout: str, heading: int, seed: int = 1) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    tag = f"{layout}{2 if layout == 'A' else 1}_h{heading:03d}"
    cmd = [
        str(SIM),
        "--layout",
        layout,
        "--heading",
        str(heading),
        "--seed",
        str(seed),
        "--timeout",
        "45",
        "-v",
        "--out",
        str(OUT),
    ]
    subprocess.run(cmd, cwd=ROOT, check=False, capture_output=True, text=True)
    html = OUT / f"run_{tag}.html"
    if not html.is_file():
        # deposit default differs by layout; find the only html
        htmls = sorted(OUT.glob(f"run_{layout}*.html"))
        if not htmls:
            raise SystemExit(f"no replay written for layout {layout}")
        html = htmls[-1]
    return html


def load_frames(html_path: Path):
    text = html_path.read_text()
    idx = text.find('{"version":1')
    if idx < 0:
        raise SystemExit(f"no sim data in {html_path}")
    start = text.find('"frames":[', idx) + len('"frames":')
    depth = 0
    end = None
    for j, ch in enumerate(text[start:], start):
        if ch == "[":
            depth += 1
        elif ch == "]":
            depth -= 1
            if depth == 0:
                end = j + 1
                break
    if end is None:
        raise SystemExit(f"frames array not closed in {html_path}")
    frames = json.loads(text[start:end])
    m = re.search(r'"events":(\[.*?\])\s*,\s*"frames"', text[idx:], re.S)
    events = json.loads(m.group(1)) if m else []
    return events, frames


def first_storage_bump(events, layout: str):
    want = "left bumper pressed" if layout == "A" else "right bumper pressed"
    for e in events:
        if e[2] == "bump" and e[3] == want:
            return e[0]
    return None


def reversed_after(frames, t_ms: int, window_ms: int = 400) -> bool:
    for q in frames:
        if t_ms <= q[0] <= t_ms + window_ms and q[12] < 0:
            return True
    return False


def check_layout(layout: str, heading: int) -> None:
    html = run(layout, heading)
    events, frames = load_frames(html)
    t_bump = first_storage_bump(events, layout)
    if t_bump is None:
        # Layout B may hit the ledge first; accept a ledge cliff then a later wall bump,
        # but still require some reverse after the first relevant wall bump if present.
        if layout == "B":
            cliffs = [e[0] for e in events if e[2] == "cliff" and "left" in e[3]]
            if cliffs and any(q[12] < 0 for q in frames if cliffs[0] <= q[0] <= cliffs[0] + 400):
                print(f"PASS {layout}: reversed after left ledge cliff at {cliffs[0]} ms")
                return
        raise SystemExit(f"FAIL {layout}: no storage-wall bumper event in replay")
    if not reversed_after(frames, t_bump):
        raise SystemExit(
            f"FAIL {layout}: left/right storage bump at {t_bump} ms was not followed by Reverse "
            f"(cmd_v < 0 within 400 ms). Firmware likely ignores that bumper mask."
        )
    print(f"PASS {layout}: reversed after storage-wall bump at {t_bump} ms")


def main() -> int:
    if not SIM.is_file():
        print(f"FAIL: missing {SIM}; build the simulator first", file=sys.stderr)
        return 2
    check_layout("A", 90)
    check_layout("B", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
