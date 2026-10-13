#!/usr/bin/env bash
# Format (or check the formatting of) this project's own C sources.
#
#   scripts/format.sh          reformat files in place
#   scripts/format.sh --check  report files that need formatting, change nothing (used by CI)
#
# Vendor / generated code is left alone: everything under Inc/drivers (ST HAL, CMSIS,
# BSP) plus the CubeIDE-generated syscall.c and sysmem.c.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

mapfile -t files < <(
    git ls-files 'Src/*.c' 'Src/*.h' \
        | grep -v -e '^Src/syscall\.c$' -e '^Src/sysmem\.c$'
)

if [ "${1:-}" = "--check" ]; then
    "$CLANG_FORMAT" --dry-run --Werror "${files[@]}"
    echo "clang-format: ${#files[@]} files OK"
else
    "$CLANG_FORMAT" -i "${files[@]}"
    echo "clang-format: formatted ${#files[@]} files"
fi
