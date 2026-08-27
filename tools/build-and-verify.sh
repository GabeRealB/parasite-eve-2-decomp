#!/usr/bin/env bash
# Format project sources, then build and verify the matching binary.
#
# Formats:
#   - src/**/*.{c,h}
#   - include/main/**/*.{c,h}
# Skips:
#   - include/psyq/**
#   - headers/macros at include/ root (include_asm.h, macro.inc, …)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Prefer newer clang-format (project .clang-format uses recent keys).
CLANG_FORMAT=""
for candidate in \
    clang-format \
    clang-format-22 \
    clang-format-21 \
    clang-format-20 \
    clang-format-19 \
    clang-format-18 \
    clang-format-17 \
    clang-format-14
do
    if command -v "$candidate" >/dev/null 2>&1; then
        CLANG_FORMAT="$candidate"
        break
    fi
done

if [[ -z "$CLANG_FORMAT" ]]; then
    echo "BUILD HAS FAILED. clang-format not found (install clang-format)."
    exit 1
fi

# Format src/ plus every overlay include dir (main, gameplay, title, future
# nested units). Skip include/psyq, include/decomp, and headers at include/.
FORMAT_ROOTS=(src)
while IFS= read -r dir; do
    FORMAT_ROOTS+=("$dir")
done < <(
    find include -mindepth 1 -maxdepth 1 -type d \
        ! -name psyq ! -name decomp 2>/dev/null | sort
)

mapfile -t FORMAT_FILES < <(
    find "${FORMAT_ROOTS[@]}" \
        \( -name '*.c' -o -name '*.h' \) \
        -type f \
        2>/dev/null | sort
)

if [[ ${#FORMAT_FILES[@]} -eq 0 ]]; then
    echo "BUILD HAS FAILED. No source files found to format under src/ or include/main/."
    exit 1
fi

# Resolve style: use .clang-format as-is when the tool understands it; otherwise
# drop keys added after older clang-format releases so format still runs.
STYLE_ARGS=(--style=file)
STYLE_TMP=""
if ! "$CLANG_FORMAT" --dry-run --style=file "${FORMAT_FILES[0]}" >/dev/null 2>&1; then
    STYLE_TMP="$(mktemp)"
    # Strip keys unknown to older clang-format (e.g. 17).
    # AlignFunctionPointers appeared in later 18+/19+ configs.
    sed -E \
        -e '/^[[:space:]]*AlignFunctionPointers:/d' \
        -e '/^[[:space:]]*AlignFunctionDeclarations:/d' \
        "$ROOT/.clang-format" >"$STYLE_TMP"
    STYLE_ARGS=(--style="file:${STYLE_TMP}")
    trap 'rm -f "$STYLE_TMP"' EXIT
fi

"$CLANG_FORMAT" -i "${STYLE_ARGS[@]}" "${FORMAT_FILES[@]}"

python3 ninja_config.py -c 1>/dev/null \
    && python3 ninja_config.py 1>/dev/null \
    && ninja 1>/dev/null \
    && (echo "✅ BUILD SUCCEEDED. Everything matched and there were no compiler or linter errors") \
    || (echo "BUILD HAS FAILED. Claude, you should treat this as a build failure. Adding new warnings or accepting a non-matching checksum count as failures." && false)
