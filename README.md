# MicroControllingProject

- Hiten
- Safin
- Shaheer M

## Docs

| Doc | What it is |
| --- | --- |
| [`GLOSSARY.md`](GLOSSARY.md) | Shared vocabulary for the FSM, report, and code |
| [`docs/pathfind.md`](docs/pathfind.md) | Pathfind states and how Drive / Reverse / Rotate work |
| [`docs/amu_fsm.drawio`](docs/amu_fsm.drawio) | Full AMU FSM diagram (edit in diagrams.net) |
| [`docs/amu_fsm_preview.png`](docs/amu_fsm_preview.png) | PNG preview of that diagram |

Pathfind recovery is **Drive → Reverse (−50 mm) → Rotate (A +10° / B −10°) → Drive**. There is no Realign, Wall hit, or Ledge hit state.
