#!/bin/bash

# Function to display usage
usage() {
    echo "Usage: $0 [--clean] [--run] [--timeout SEC] [-j THREADS] FUNCTION_NAME ASM_FUNCTION C_FUNCTION"
    echo "  --clean       : Clean the output directory before processing"
    echo "  --run         : Run the permuter after setup (stops on score 0)"
    echo "  --timeout SEC : Kill --run after SEC seconds"
    echo "  -j THREADS    : Number of threads to use for the permuter (default: 4)"
    echo "  FUNCTION_NAME : Name of the function to permute"
    echo "  ASM_FUNCTION  : Path to the .s file from asm/../nonmatching folder"
    echo "  C_FUNCTION    : Path to the C file containing just the function and related includes"
    exit 1
}

# Initialize flags
CLEAN=false
RUN=false
THREADS=4
TIMEOUT=""

# Small-data threshold. Must match ninja_config.py, or the permuter compiles
# with gp-relative addressing the target does not use.
G_OPTION="-G0"

# Parse flags. `-j4` and `-j 4` both have to work: every caller-facing doc
# (CLAUDE.md, MATCH_LOOP.md, the vacuum's agent prompts) writes the attached
# form, and matching only `-j` made the loop exit on it, so FUNCTION_NAME
# silently became "-j4" and every later argument shifted by one.
while [[ "$1" == --* || "$1" == -j* ]]; do
    case "$1" in
        --clean)
            CLEAN=true
            shift
            ;;
        --run)
            RUN=true
            shift
            ;;
        --timeout)
            if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
                TIMEOUT="$2"
                shift 2
            else
                echo "Error: --timeout requires a numeric argument (seconds)"
                usage
            fi
            ;;
        -j)
            if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
                THREADS="$2"
                shift 2
            else
                echo "Error: -j requires a numeric argument"
                usage
            fi
            ;;
        -j*)
            if [[ "${1#-j}" =~ ^[0-9]+$ ]]; then
                THREADS="${1#-j}"
                shift
            else
                echo "Error: -j requires a numeric argument"
                usage
            fi
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Get the parameters
FUNCTION_NAME=$1
ASM_FUNCTION=$2
C_FUNCTION=$3

# Validate parameters
if [ -z "$FUNCTION_NAME" ] || [ -z "$ASM_FUNCTION" ] || [ -z "$C_FUNCTION" ]; then
    usage
fi

# Get the path to the current script's directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# decomp-permuter is an upstream submodule, so our objdump fix cannot be
# committed as a file and does not survive `git submodule update`. Keep it as a
# tracked patch and re-apply it whenever the submodule comes back pristine.
# Without it, scoring dies outright on any function where GCC emits a `j` to a
# label inside itself - objdump prints that target as bare hex and int(imm, 0)
# raises. Long switch bodies are exactly what produces those jumps.
PERMUTER_PATCH="$SCRIPT_DIR/tools/decomp-permuter-objdump.patch"
if [ -f "$PERMUTER_PATCH" ] &&
    ! grep -q "def parse_imm" "$SCRIPT_DIR/tools/decomp-permuter/src/objdump.py" 2>/dev/null; then
    if git -C "$SCRIPT_DIR/tools/decomp-permuter" apply "$PERMUTER_PATCH" 2>/dev/null; then
        echo "Applied objdump patch to tools/decomp-permuter"
    else
        echo "Warning: could not apply $PERMUTER_PATCH; scoring may crash on switch-heavy functions" >&2
    fi
fi

# Clean the directory if the --clean flag is set
if [ "$CLEAN" = true ]; then
    echo "Cleaning directory: permuter/$FUNCTION_NAME"
    rm -rf permuter/$FUNCTION_NAME
fi

echo "Creating directory: permuter/$FUNCTION_NAME"
mkdir -p permuter/$FUNCTION_NAME

echo "Creating settings.toml file"
{
    echo "func_name = \"$FUNCTION_NAME\""
    cat permute_defaults.toml
} > permuter/$FUNCTION_NAME/settings.toml

echo "Creating compile.sh file"
{
    echo '#!/usr/bin/env bash'
    echo 'INPUT="$(realpath "$1")"'
    echo 'INPUT_I=$INPUT.i'
    echo 'INPUT_D=$INPUT_I.d'
    echo 'INPUT_S=$INPUT_I.s'
    echo ''
    echo 'OUTPUT="$(realpath "$3")"'
    echo ''
    echo "cd $SCRIPT_DIR"
    echo ''
    echo 'mips-linux-gnu-cpp -P -MMD -MP -MT "$INPUT_I" -MF "$INPUT_D" -Iinclude -Iinclude/psyq -I build -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -undef -Wall -lang-c -nostdinc -o "$INPUT_I" "$INPUT"'
    echo "tools/linux/gcc-2.8.1-psx/cc1 -O2 $G_OPTION -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet -o \"\$INPUT_S\" \"\$INPUT_I\""
    echo "python3 tools/maspsx/maspsx.py --aspsx-version=2.77 --run-assembler --expand-div -EL -Iinclude -Iinclude/psyq -I build -O2 $G_OPTION -march=r3000 -mtune=r3000 -no-pad-sections -o \"\$OUTPUT\" \"\$INPUT_S\" < /dev/null"
} > permuter/$FUNCTION_NAME/compile.sh

chmod +x permuter/$FUNCTION_NAME/compile.sh

echo "Creating or overwriting permuter/$FUNCTION_NAME/target.s file from $ASM_FUNCTION"
# prelude.inc is not optional. Without `.set noreorder` gas fills the target's
# delay slots and inserts its own nops, so the target we score against is not
# the target at all: a seed that build.sh scores 99.512% scored 70.141% here,
# and the permuter then spent 24k iterations chasing a stream it could never
# reach. Assemble the target exactly the way the scratch env does.
{
    cat "$SCRIPT_DIR/tools/claude-decomp-env/prelude.inc"
    echo '.include "macro.inc"'
    echo ''
    echo ''
    cat $ASM_FUNCTION
} > permuter/$FUNCTION_NAME/target.s

echo "Preprocessing the C function $C_FUNCTION and saving to permuter/$FUNCTION_NAME/base.c"
gcc -E -P -Iinclude -Iinclude/decomp -Iinclude/psyq -DPERMUTER $C_FUNCTION > permuter/$FUNCTION_NAME/base.c

echo "Assembling the ASM function permuter/$FUNCTION_NAME/target.o"
mips-linux-gnu-as -EL -Iinclude -Iinclude/decomp -Iinclude/psyq -I build -O2 -march=r3000 -mtune=r3000 -no-pad-sections $G_OPTION -o permuter/$FUNCTION_NAME/target.o permuter/$FUNCTION_NAME/target.s

# Run decomp-permuter if the --run flag is set
if [ "$RUN" = true ]; then

    # The permuter leaves a scratch file per candidate compile behind in TMPDIR
    # (~24 a second). Give it a private one that goes away with the run. This
    # used to be a background loop deleting /tmp/permuter* older than two
    # minutes, which only ran under --run: invoking permuter.py directly, as
    # tools/vacuum_permute.py does, leaked unchecked and filled a 12G tmpfs in
    # one long search, after which everything needing /tmp fails on ENOSPC.
    PERM_TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/permuter-${FUNCTION_NAME}-XXXXXX")"
    trap 'rm -rf "$PERM_TMPDIR"' EXIT
    export TMPDIR="$PERM_TMPDIR"

    echo "Running decomp-permuter"
    PYTHON="$SCRIPT_DIR/venv/bin/python"
    if [[ ! -x "$PYTHON" ]]; then
        PYTHON="python3"
    fi
    PERM_CMD=(
        "$PYTHON" tools/decomp-permuter/permuter.py
        -j"$THREADS"
        --better-only
        --stop-on-zero
        --no-context-output
        --algorithm levenshtein
        "permuter/$FUNCTION_NAME/"
    )
    if [[ -n "$TIMEOUT" ]]; then
        timeout --kill-after=20s "$TIMEOUT" "${PERM_CMD[@]}"
    else
        "${PERM_CMD[@]}"
    fi
fi
