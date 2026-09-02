#!/bin/bash
# build.sh $file_to_compile.c
# takes a .c file, compiles it, and diffs the resulting object code against the target assembly
set -e

INPUT="$(realpath "$1")"
CPP_OUTPUT="$(realpath "${1//.c/.i}")"
CC_OUTPUT="$(realpath "${1//.c/.s}")"
MASPSX_OUTPUT="$(realpath "${1//.c/.o}")"
ANNOTATED_OUTPUT="$(realpath "${1//.c/_annotated.s}")"
OBJECT_DUMP="${1//.c/_object_dump.s}"

# Check if file contains INCLUDE_ASM
if grep -q "INCLUDE_ASM" "$INPUT"; then
    echo "ERROR: The C file contains INCLUDE_ASM macro!"
    echo ""
    echo "INCLUDE_ASM is NOT a valid decompilation technique."
    echo "You must write actual C code that compiles to matching assembly."
    echo ""
    echo "Using INCLUDE_ASM is an attempt to cheat the matching process by"
    echo "embedding assembly directly. This defeats the entire purpose of"
    echo "decompilation, which is to produce readable, maintainable C code."
    echo ""
    echo "Please rewrite your code as proper C without any INCLUDE_ASM macros."
    exit 1
fi

# Set project root to two directories above this script
SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_PATH/../.." && pwd)"

# Compiler settings (ninja_config)
MASPSX="$PROJECT_ROOT/tools/maspsx/maspsx.py"
CROSS="mips-linux-gnu"
OBJDUMP="${CROSS}-objdump"
CPP="${CROSS}-cpp"
NM="${CROSS}-nm"
CC="$PROJECT_ROOT/tools/linux/gcc-2.8.1-psx/cc1"

INCLUDE_FLAGS="-I $PROJECT_ROOT/include -I $PROJECT_ROOT/build -I $PROJECT_ROOT/include/psyq -I $PROJECT_ROOT/include/decomp -I $PROJECT_ROOT/tools/m2c"
OPT_FLAGS="-O2"
ENDIAN="-EL"
MASPX_VERSION="2.77"

CPP_FLAGS="$INCLUDE_FLAGS -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -undef -Wall -lang-c -nostdinc"
MASPSX_FLAGS="--aspsx-version=${MASPX_VERSION} --run-assembler --expand-div"
CC_FLAGS="$OPT_FLAGS -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet -gcoff -dp"
AS_FLAGS="$ENDIAN $INCLUDE_FLAGS $OPT_FLAGS -march=r3000 -mtune=r3000 -no-pad-sections"
OBJDUMP_FLAGS="--disassemble-all --reloc --disassemble-zeroes -Mreg-names=32"

# Compile from project root so assembler can find include/labels.inc
pushd "$PROJECT_ROOT" > /dev/null

# Run cpp
$CPP -P -MMD -MP -MT "$CPP_OUTPUT" -MF "${CPP_OUTPUT}.d" $CPP_FLAGS \
    -DVER_USA -DSKIP_ASM -DNON_MATCHING \
    -o "$CPP_OUTPUT" "$INPUT"

# Run cc
$CC $CC_FLAGS -G0 -o "$CC_OUTPUT" "$CPP_OUTPUT"
rm -f "$CPP_OUTPUT"
rm -f "${CPP_OUTPUT}.d"

# Run maspsx. Keep the .s: -fverbose-asm / -dp comments map each insn to
# an RTL uid (e.g. `lw $2,0($4)  # 31 movsi_internal2/5`). dump.sh uses
# the same uids. If this .s already matches and the .o does not, the bug
# is maspsx, not cc1.
python3 "$MASPSX" $MASPSX_FLAGS $AS_FLAGS -G0 -o "$MASPSX_OUTPUT" "$CC_OUTPUT" < /dev/null

# Generate annotated assembly
$OBJDUMP -d --line-numbers --reloc --source "$MASPSX_OUTPUT" > "$ANNOTATED_OUTPUT"

popd > /dev/null

# Validate the compiled object has actual code
if ! $NM "$MASPSX_OUTPUT" 2>/dev/null | grep -q ' T '; then
    echo "ERROR: Compiled object has no text symbols. Check for type conflicts or include issues."
    echo "Symbols found:"
    $NM "$MASPSX_OUTPUT" 2>/dev/null || true
    exit 1
fi

python3 ./objdump.py target.o > target_object_dump.s
python3 ./objdump.py $MASPSX_OUTPUT > $OBJECT_DUMP
echo "cc1 assembly with insn uids: $CC_OUTPUT"
echo "Raw decompiled assembly of $1: ${1//.c/_object_dump.s}"
echo "Decompiled assembly of $1 with C annotations: $ANNOTATED_OUTPUT"

echo $OBJECT_DUMP
echo ${1//.c/_diff}
python3 ./normalize_asm.py target_object_dump.s > target_object_dump_normalized.s
python3 ./normalize_asm.py $OBJECT_DUMP > ${1//.c/_object_dump_normalized.s}
diff -u --suppress-common-lines target_object_dump_normalized.s ${1//.c/_object_dump_normalized.s} > ${1//.c/_diff} || true
echo "Comparison with target file: ${1//.c/_diff}"

SCORE_OUTPUT=$(python3 dist.py target.o $MASPSX_OUTPUT --stack-diffs)
echo "$SCORE_OUTPUT"

# Extract match percentage and log it (only for base_* files)
MATCH_PERCENT=$(echo "$SCORE_OUTPUT" | grep -oP 'Score: \K[0-9.]+')
PENALTIES=$(echo "$SCORE_OUTPUT" | grep -oP 'Penalties: \K.*' || true)
if [[ $1 =~ base_[0-9]+ ]]; then
    echo "$1 ${MATCH_PERCENT}% ${PENALTIES}" >> match_log.txt

    # Stall detection: warn if no progress in last 40 attempts
    if [[ -f match_log.txt ]]; then
        STALL_INFO=$(awk '
        {
            gsub(/%/, "", $2)
            total++
            if ($2 + 0 > best + 0) {
                best = $2 + 0
                best_file = $1
                best_at = total
            }
        }
        END {
            since = total - best_at
            if (since > 40) {
                printf "%d %s %.1f\n", since, best_file, best
            }
        }' match_log.txt)
        if [[ -n "$STALL_INFO" ]]; then
            read -r SINCE BEST_FILE BEST_SCORE <<< "$STALL_INFO"
            echo "🛑 No progress in $SINCE attempts (best: ${BEST_SCORE}% at $BEST_FILE). STOP — do not make another attempt. Report your findings immediately."
        fi
    fi
fi

# Auto-dump when close: regs/reorder leftovers need .lreg/.greg/.sched/.dbr,
# not more C rewrites. dump.sh is a second cc1 -da; skip on a perfect match
# and on low scores where control flow is still the problem.
if [[ -n "$MATCH_PERCENT" ]] && awk -v p="$MATCH_PERCENT" 'BEGIN { exit !(p + 0 >= 90 && p + 0 < 100) }'; then
    echo "Score ≥ 90% — running ./dump.sh $1 (RTL summary below). Do not add register pins yet."
    "$SCRIPT_PATH/dump.sh" "$INPUT" || echo "dump.sh failed (score above still stands)"

    # Name the dump files the next tool call must open. Penalty mix decides;
    # the printed summary is not a substitute for reading these files.
    stem="${1%.c}"
    branch=$(echo "$PENALTIES" | grep -oP 'branch=\K[0-9]+' || echo 0)
    regs=$(echo "$PENALTIES" | grep -oP 'regs=\K[0-9]+' || echo 0)
    reorder=$(echo "$PENALTIES" | grep -oP 'reorder=\K[0-9]+' || echo 0)
    insert=$(echo "$PENALTIES" | grep -oP 'insert=\K[0-9]+' || echo 0)
    delete=$(echo "$PENALTIES" | grep -oP 'delete=\K[0-9]+' || echo 0)
    stack=$(echo "$PENALTIES" | grep -oP 'stack=\K[0-9]+' || echo 0)
    echo "NEXT: open these dump files before editing C (summary is not enough):"
    if awk -v b="$branch" -v i="$insert" -v d="$delete" 'BEGIN { exit !((b+i+d) > 0) }'; then
        echo "  ${stem}.i.jump  ${stem}.i.jump2    (control flow still wrong — fix C shape, do not pin)"
    fi
    if awk -v r="$regs" 'BEGIN { exit !(r > 0) }'; then
        echo "  ${stem}.i.lreg  ${stem}.i.greg     (split a reused local or unpin; do not add register asm pins)"
    fi
    if awk -v r="$reorder" 'BEGIN { exit !(r > 0) }'; then
        echo "  ${stem}.i.sched  ${stem}.i.sched2  ${stem}.i.dbr    (statement order / delay slots)"
    fi
    if awk -v s="$stack" 'BEGIN { exit !(s > 0) }'; then
        echo "  extra locals / frame — split or shrink locals"
    fi
    echo "Do not add register … asm(\"\") pins. A non-zero branch/insert/delete score is not a register-coloring problem."
fi

if grep -qE 'register[[:space:]][^;]+asm[[:space:]]*\(' "$INPUT" 2>/dev/null; then
    echo "PIN WARNING: $1 contains register … asm(\"\"). Function-scope pins reserve that hard register for the whole function. Unpin and rescore as its own base_N.c before treating this as the best seed."
fi
