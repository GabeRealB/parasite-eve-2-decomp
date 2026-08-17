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

INCLUDE_FLAGS="-I $PROJECT_ROOT/include -I $PROJECT_ROOT/build -I $PROJECT_ROOT/include/psyq -I $PROJECT_ROOT/include/decomp"
OPT_FLAGS="-O2"
ENDIAN="-EL"
MASPX_VERSION="2.77"

CPP_FLAGS="$INCLUDE_FLAGS -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -undef -Wall -lang-c -nostdinc"
MASPSX_FLAGS="--aspsx-version=${MASPX_VERSION} --run-assembler --expand-div"
CC_FLAGS="$OPT_FLAGS -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet -gcoff"
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

# Run maspsx
python3 "$MASPSX" $MASPSX_FLAGS $AS_FLAGS -G0 -o "$MASPSX_OUTPUT" "$CC_OUTPUT" < /dev/null
rm -f "$CC_OUTPUT"

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
if [[ $1 =~ base_[0-9]+ ]]; then
    echo "$1 ${MATCH_PERCENT}%" >> match_log.txt

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
