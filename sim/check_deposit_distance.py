#!/usr/bin/env python3
"""Build and run the deposit control-loop checks without memory-mapped hardware."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    includes = [ROOT / "sim/include"]
    includes.extend(sorted(path for path in (ROOT / "Src").rglob("*") if path.is_dir()))
    includes.append(ROOT / "Inc/drivers/CMSIS/Device/ST/STM32G4xx/Include")
    with tempfile.TemporaryDirectory(prefix="deposit-distance-") as temp_dir:
        binary = Path(temp_dir) / ("check.exe" if os.name == "nt" else "check")
        command = [
            os.environ.get("CC", "gcc"),
            "-std=gnu11",
            "-Wall",
            "-Wextra",
            "-Wno-enum-conversion",
            "-DSTM32G474xx",
            "-DDEPOSIT_TEST_MODE=1",
            *[f"-I{path}" for path in includes],
            str(ROOT / "sim/check_deposit_distance.c"),
            str(ROOT / "Src/Helper/helper.c"),
            "-o",
            str(binary),
            "-lm",
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
