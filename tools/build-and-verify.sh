#!/usr/bin/env bash
# Format project sources, then build and verify the matching binary.
#
# Usage: build-and-verify.sh [--only SELECTOR[,SELECTOR...]]
#
# With --only, splits and builds just those units and checksums only what it
# built - a family (core, weapons) or a single basename (gameplay, m93r).
# Other overlays' asm/ and linkers/ are left alone, so a scoped run is the fast
# inner loop while matching one function; finish with an unscoped run before
# calling anything done.
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

SCOPE_ARGS=()
SCOPE_LABEL=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --only|-o)
            [[ $# -ge 2 ]] || { echo "build-and-verify.sh: --only needs a value" >&2; exit 2; }
            SCOPE_ARGS+=(--only "$2"); SCOPE_LABEL="${SCOPE_LABEL:+$SCOPE_LABEL,}$2"; shift 2 ;;
        --only=*)
            SCOPE_ARGS+=(--only "${1#*=}"); SCOPE_LABEL="${SCOPE_LABEL:+$SCOPE_LABEL,}${1#*=}"; shift ;;
        -h|--help)
            sed -n "2,12p" "$0"; exit 0 ;;
        *)
            echo "build-and-verify.sh: unknown argument '$1'" >&2
            echo "usage: build-and-verify.sh [--only SELECTOR[,SELECTOR...]]" >&2
            exit 2 ;;
    esac
done

# Prefer the project venv. Agent CLIs (especially grok) often invoke this
# script with a PATH that has system python3 and no splat/spimdisasm.
PYTHON="python3"
if [[ -x "$ROOT/venv/bin/python3" ]]; then
    PYTHON="$ROOT/venv/bin/python3"
elif [[ -x "$ROOT/.venv/bin/python3" ]]; then
    PYTHON="$ROOT/.venv/bin/python3"
fi

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

# A full run starts from a clean build tree. A scoped run must not: wiping
# build/ would throw away exactly the incremental work that makes it fast.
if [[ ${#SCOPE_ARGS[@]} -eq 0 ]]; then
    "$PYTHON" ninja_config.py -c 1>/dev/null
    SUCCESS="✅ BUILD SUCCEEDED. Everything matched and there were no compiler or linter errors"
else
    SUCCESS="✅ SCOPED BUILD SUCCEEDED (${SCOPE_LABEL}). Only these units were split, built and checksummed - run without --only before treating the project as matching."
fi

# ninja reports a failed command - the compiler diagnostic, the linker's
# "cannot find …", a checksum mismatch - on *stdout*. Sending that to /dev/null
# left "BUILD HAS FAILED" as the only thing said about any failure, with the
# real cause discarded and whatever splat had last written to stderr looking
# like the error. Keep it, and print it when the build fails.
NINJA_LOG="$(mktemp -t pe2-ninja-XXXXXX.log)"
ninja_failed=0
"$PYTHON" ninja_config.py "${SCOPE_ARGS[@]+"${SCOPE_ARGS[@]}"}" 1>/dev/null || ninja_failed=1
if [[ $ninja_failed -eq 0 ]]; then
    ninja >"$NINJA_LOG" 2>&1 || ninja_failed=1
fi

if [[ $ninja_failed -eq 0 ]]; then
    rm -f "$NINJA_LOG"
    echo "$SUCCESS"
else
    grep -E "^(FAILED|ninja:)|error:|Error:|undefined reference|multiple definition|cannot find|does not match" "$NINJA_LOG" | head -40
    echo "(full ninja output: $NINJA_LOG)"
    echo "BUILD HAS FAILED. Claude, you should treat this as a build failure. Adding new warnings or accepting a non-matching checksum count as failures."
    exit 1
fi
