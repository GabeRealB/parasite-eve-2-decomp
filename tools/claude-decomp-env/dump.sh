#!/bin/bash
# dump.sh $file_to_compile.c
# Re-run GCC 2.8.1 cc1 with -da -dp. Does not assemble (no maspsx).
# Dumps land next to the .i: file.i.rtl, .cse, .combine, .loop, .lreg, .greg,
# .sched, .sched2, .jump, .jump2, .dbr, .cse2, .flow, ...
set -e

if [[ $# -lt 1 || "$1" != *.c ]]; then
    echo "Usage: $0 <file.c>"
    exit 1
fi

INPUT="$(realpath "$1")"
CPP_OUTPUT="$(realpath "${1//.c/.i}")"
CC_OUTPUT="$(realpath "${1//.c/.s}")"

SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_PATH/../.." && pwd)"

CC="$PROJECT_ROOT/tools/linux/gcc-2.8.1-psx/cc1"
CPP="mips-linux-gnu-cpp"

INCLUDE_FLAGS="-I $PROJECT_ROOT/include -I $PROJECT_ROOT/build -I $PROJECT_ROOT/include/psyq -I $PROJECT_ROOT/include/decomp"
OPT_FLAGS="-O2"
CPP_FLAGS="$INCLUDE_FLAGS -D_LANGUAGE_C -DUSE_INCLUDE_ASM -P -MMD -MP -undef -Wall -lang-c -nostdinc"
CC_FLAGS="$OPT_FLAGS -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet -gcoff -dp"

pushd "$PROJECT_ROOT" > /dev/null

$CPP -P -MMD -MP -MT "$CPP_OUTPUT" -MF "${CPP_OUTPUT}.d" $CPP_FLAGS \
    -DVER_USA -DSKIP_ASM -DNON_MATCHING \
    -o "$CPP_OUTPUT" "$INPUT"
rm -f "${CPP_OUTPUT}.d"

# -da writes one file per pass next to the .i. -dp annotates the .s with
# RTL insn uids (`# 31 movsi_internal2/5`) that join objdump to those dumps.
$CC $CC_FLAGS -G0 -da -o "$CC_OUTPUT" "$CPP_OUTPUT"

popd > /dev/null

echo "cc1 assembly with insn uids: $CC_OUTPUT"
echo "Preprocessed input (dumps are named after this): $CPP_OUTPUT"
echo ""
python3 "$SCRIPT_PATH/summarize_dumps.py" "$CPP_OUTPUT" | tee DUMP.txt
echo ""
echo "Which dump to open:"
echo "  regs swapped / wrong \$sN     .lreg .greg"
echo "  fused const, lb vs lh, andi   .cse .cse2 .combine"
echo "  loop IV / walking pointer     .loop then .cse2"
echo "  insn order, load-delay nop    .sched .sched2"
echo "  empty delay slot after jal    .dbr"
echo "  merged tails, extra j         .jump .jump2"
echo "  dead store / REG_DEAD         .flow"
echo "If this .s already matches target order and the .o does not, the bug is maspsx."
