#!/usr/bin/env bash
set -euo pipefail

SIM="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SIM/.." && pwd)"
SRC="$ROOT/Src"
OBJ="$SIM/obj"
EXE="$SIM/kobuki_sim"

if ! command -v gcc >/dev/null 2>&1; then
    echo "ERROR: gcc not found." >&2
    exit 1
fi
if [[ ! -d "$SRC" ]]; then
    echo "ERROR: Source folder not found: $SRC" >&2
    exit 1
fi

rm -rf "$OBJ"
mkdir -p "$OBJ"

INCS="$OBJ/includes.rsp"
FWS="$OBJ/firmware.rsp"
OBS="$OBJ/objects.rsp"

add_tree() {
    local root="$1"
    local d
    [[ -d "$root" ]] || return 0
    printf '"-I%s"\n' "$root"
    while IFS= read -r d; do
        printf '"-I%s"\n' "$d"
    done < <(find "$root" -type d | sort)
}

{
    printf '"-I%s"\n' "$SIM/include"
    add_tree "$SRC"
    add_tree "$ROOT/Inc"
    add_tree "$ROOT/Drivers"
} > "$INCS"

: > "$FWS"
count=0
while IFS= read -r f; do
    base="$(basename "$f")"
    case "$base" in
        syscall.c|sysmem.c|uart.c|adc.c) continue ;;
    esac
    printf '"%s"\n' "$f" >> "$FWS"
    count=$((count + 1))
done < <(find "$SRC" -name '*.c' -type f | sort)

if [[ "$count" -eq 0 ]]; then
    echo "ERROR: no firmware .c files found in $SRC." >&2
    exit 1
fi

echo "Compiling ${count} firmware files from Src/ ..."
(
    cd "$OBJ"
    gcc -std=gnu11 -O1 -g -DSTM32G474xx -c -Dmain=firmware_main \
        -Wno-int-to-pointer-cast \
        -include "$SIM/include/sim_nvic_compat.h" \
        @"$INCS" @"$FWS"
)

: > "$OBS"
while IFS= read -r o; do
    printf '"%s"\n' "$o"
done < <(find "$OBJ" -name '*.o' -type f | sort) > "$OBS"

echo "Linking simulator ..."
gcc -std=gnu11 -O2 -g -DSTM32G474xx -DSIM_SHIM -Wall -pthread \
    -Wno-int-to-pointer-cast -Wno-format-truncation \
    @"$INCS" -I"$SIM/src" \
    "$SIM/src/sim_main.c" \
    "$SIM/src/sim_mmio_posix.c" \
    "$SIM/src/sim_periph.c" \
    "$SIM/src/sim_robot.c" \
    "$SIM/src/sim_shim.c" \
    "$SIM/src/sim_host_posix.c" \
    @"$OBS" -o "$EXE" -lm

echo "Built $EXE"
