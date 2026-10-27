#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SIM="$ROOT/sim"
SRC="$ROOT/Src"
OBJ="$SIM/obj_mingw"
EXE="$SIM/kobuki_sim_mingw.exe"
CC="${MINGW_CC:-x86_64-w64-mingw32-gcc}"

rm -rf "$OBJ"
mkdir -p "$OBJ"

{
  echo "-I$SIM/include"
  echo "-I$SRC"
  echo "-I$ROOT/Inc"
  find "$SRC" -type d | sed 's/^/-I/'
  find "$ROOT/Inc" -type d 2>/dev/null | sed 's/^/-I/'
} >"$OBJ/includes.rsp"

find "$SRC" -name '*.c' ! -name 'syscall.c' ! -name 'sysmem.c' | sort >"$OBJ/firmware.list"

while IFS= read -r f; do
  base=$(basename "$f" .c)
  "$CC" -std=gnu11 -O1 -g -DSTM32G474xx -c -Dmain=firmware_main -Wno-int-to-pointer-cast \
    -include "$SIM/include/sim_nvic_compat.h" @"$OBJ/includes.rsp" "$f" -o "$OBJ/$base.o"
done <"$OBJ/firmware.list"

"$CC" -std=gnu11 -O2 -g -DSTM32G474xx -Wall -Wno-int-to-pointer-cast \
  @"$OBJ/includes.rsp" -I"$SIM/src" \
  "$SIM/src/sim_main.c" "$SIM/src/sim_mmio.c" "$SIM/src/sim_periph.c" "$SIM/src/sim_robot.c" \
  "$OBJ"/*.o -o "$EXE" -lm

echo "Built $EXE"
