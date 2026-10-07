#ifndef GTE_H
#define GTE_H

/// GTE command macros that emit real COP2 instructions.
///
/// Psy-Q's `inline_c.h` writes every GTE *command* as two `nop`s followed by a
/// placeholder word (`gte_rtps()` emits `.word 0x0000007f`). Sony's DMPSX tool
/// then rewrote each placeholder in the compiled object into the real COP2
/// instruction, and the game was built from that output. This toolchain has no
/// DMPSX step, so this header takes its place: it keeps Psy-Q's macro names and
/// its `nop; nop;` prefix, and substitutes the instruction word DMPSX would
/// have produced. Code can then call the SDK macros the way the original source
/// did, including through the compound macros in `gtemac.h`.
///
/// Only the commands change. The register load and store macros in
/// `inline_c.h` already emit real `lwc2`/`swc2`/`mtc2`/`ctc2` instructions and
/// are used unchanged. The words match the instruction table in
/// `gte_macros.inc`, the assembler-side replacement for DMPSX.
///
/// Notation in the comments below. Inputs are the vector registers V0..V2, the
/// IR vector IR1..3 with its scalar IR0, the input colour RGBC and the control
/// matrices: the rotation matrix RT with translation TR, the light matrix LLM,
/// the light colour matrix LCM and the background colour BK.
/// Results land in MAC1..3 and IR1..3 (and MAC0 for scalar results); colours
/// are pushed into the colour FIFO RGB0..2, screen points into SXY0..2 and
/// depths into SZ0..3. Products are shifted right by 12 bits for 4.12 fixed
/// point; the `0` and `_sf0` forms leave them unshifted. Commands marked
/// "clamped" limit IR1..3 to non-negative values.
///
/// Only the commands the game's code uses are defined, directly or through a
/// `gtemac.h` compound (`gte_rt` is reached only that way, from `gte_RotTrans`
/// and `gte_CompMatrix`). Every other command - the remaining MVMVA forms, the
/// depth-cue, `nc`/`ncd`, `cc`/`cdp` and unshifted (`0`) commands, and all the
/// `_b` forms that drop the leading `nop`s - keeps Psy-Q's placeholder, which
/// still compiles and assembles but is not a GTE instruction. A function that
/// needs one must add it here first, with the word from `gte_macros.inc`.

#include <psyq/inline_c.h>

/// Perspective transformation. `gte_rtps` rotates and translates V0
/// (IR1..3 = TR + RT·V0), pushes its projected screen point into SXY and its
/// depth into SZ, and sets IR0 to the depth-cue factor. `gte_rtpt` does the
/// same for V0, V1 and V2 in turn.
#undef gte_rtps
#define gte_rtps()     __asm__ volatile("nop; nop; .word 0x4A180001")
#undef gte_rtpt
#define gte_rtpt()     __asm__ volatile("nop; nop; .word 0x4A280030")
/// Matrix times vector (MVMVA): IR1..3 = [offset +] matrix · vector. The
/// name spells the operands. Matrix: `rt` the rotation matrix. Vector: `v0`,
/// `v1`, `v2` or `ir`. Offset: none, or `tr` the translation vector.
/// `gte_rt` is `gte_rtv0tr` under Psy-Q's rotate-and-translate name.
#undef gte_rt
#define gte_rt()       __asm__ volatile("nop; nop; .word 0x4A480012")
#undef gte_rtv0
#define gte_rtv0()     __asm__ volatile("nop; nop; .word 0x4A486012")
#undef gte_rtv1
#define gte_rtv1()     __asm__ volatile("nop; nop; .word 0x4A48E012")
#undef gte_rtv2
#define gte_rtv2()     __asm__ volatile("nop; nop; .word 0x4A496012")
#undef gte_rtir
#define gte_rtir()     __asm__ volatile("nop; nop; .word 0x4A49E012")
#undef gte_rtv0tr
#define gte_rtv0tr()   __asm__ volatile("nop; nop; .word 0x4A480012")
#undef gte_rtirtr
#define gte_rtirtr()   __asm__ volatile("nop; nop; .word 0x4A498012")
/// IR1..3 squared component by component, clamped and unshifted.
#undef gte_sqr0
#define gte_sqr0()     __asm__ volatile("nop; nop; .word 0x4AA00428")
/// Lighting, clamped, results pushed into the colour FIFO. The `ncc` commands
/// light V0 (`s`, single) or V0..V2 (`t`, triple) as a surface normal: the
/// light step LLM·V, the colour step BK + LCM·IR, then a multiply by the input
/// colour RGBC.
#undef gte_nccs
#define gte_nccs()     __asm__ volatile("nop; nop; .word 0x4B08041B")
#undef gte_ncct
#define gte_ncct()     __asm__ volatile("nop; nop; .word 0x4B18043F")
/// MAC0 = twice the signed area of the screen triangle SXY0..2. Its sign
/// gives the triangle's winding, which is how back faces are culled.
#undef gte_nclip
#define gte_nclip()    __asm__ volatile("nop; nop; .word 0x4B400006")
/// Average depth for the ordering table: the last three (`gte_avsz3`, scaled
/// by ZSF3) or all four (`gte_avsz4`, scaled by ZSF4) SZ entries, into OTZ.
#undef gte_avsz3
#define gte_avsz3()    __asm__ volatile("nop; nop; .word 0x4B58002D")
#undef gte_avsz4
#define gte_avsz4()    __asm__ volatile("nop; nop; .word 0x4B68002E")
/// Outer product: IR1..3 crossed with the rotation matrix's diagonal.
#undef gte_op12
#define gte_op12()     __asm__ volatile("nop; nop; .word 0x4B78000C")
/// General-purpose interpolation. `gte_gpf12` scales IR1..3 by IR0;
/// `gte_gpl12` adds that product to the current MAC1..3, so a sequence of them
/// accumulates a weighted sum. Both push the result into the colour FIFO.
#undef gte_gpf12
#define gte_gpf12()    __asm__ volatile("nop; nop; .word 0x4B98003D")
#undef gte_gpl12
#define gte_gpl12()    __asm__ volatile("nop; nop; .word 0x4BA8003E")

/// Rotation without the 12-bit fraction shift (`sf` = 0), which Psy-Q names
/// only for the IR vector (`gte_rtir_sf0`): the product of the rotation matrix
/// and V0, with no translation added.
#define gte_rtv0_sf0() __asm__ volatile("nop; nop; .word 0x4A406012")

/// Loads one register of the GTE's screen-Z FIFO (SZ0-SZ3). Psy-Q loads these
/// only as a group (`gte_ldsz3`, `gte_ldsz4`); code that tests each depth
/// before loading it uses the single loads.
#define gte_ldSZ0(r0) __asm__ volatile("mtc2 %0, $16" : : "r"(r0))
#define gte_ldSZ1(r0) __asm__ volatile("mtc2 %0, $17" : : "r"(r0))
#define gte_ldSZ2(r0) __asm__ volatile("mtc2 %0, $18" : : "r"(r0))
#define gte_ldSZ3(r0) __asm__ volatile("mtc2 %0, $19" : : "r"(r0))

/// Pushes one packed screen coordinate into the GTE's SXY FIFO (SXYP): the
/// oldest of SXY0-SXY2 drops out. Psy-Q loads SXY0-2 only as a group
/// (`gte_ldsxy3`); code that feeds a polygon's corners one at a time, or
/// re-tests one corner, pushes them singly.
#define gte_ldSXYP(r0) __asm__ volatile("mtc2 %0, $15" : : "r"(r0))

/// Writes the transpose of `src`'s rotation into `dst`, one column of `src` to
/// one row of `dst` at a time through `$12`-`$14`: the sequence of libgte's
/// `TransposeMatrix`, inlined. The translation is left alone.
#define gte_TransposeMatrix(src, dst)                     \
    __asm__ volatile("lhu $12,0(%0);"                     \
                     "lhu $13,6(%0);"                     \
                     "lhu $14,12(%0);"                    \
                     "sh $12,0(%1);"                      \
                     "sh $13,2(%1);"                      \
                     "sh $14,4(%1);"                      \
                     "lhu $12,2(%0);"                     \
                     "lhu $13,8(%0);"                     \
                     "lhu $14,14(%0);"                    \
                     "sh $12,6(%1);"                      \
                     "sh $13,8(%1);"                      \
                     "sh $14,10(%1);"                     \
                     "lhu $12,4(%0);"                     \
                     "lhu $13,10(%0);"                    \
                     "lhu $14,16(%0);"                    \
                     "sh $12,12(%1);"                     \
                     "sh $13,14(%1);"                     \
                     "sh $14,16(%1);"                     \
                     :                                    \
                     : "r"(src), "r"(dst)                 \
                     : "$12", "$13", "$14", "memory")

/// Copies column `col` (0-2) of `m`'s rotation into the SVECTOR `v`
/// (`v->vx = m->m[0][col]` ... `v->vz = m->m[2][col]`) through `$12`-`$14`.
#define gte_ReadMatrixColumn(m, col, v)                                    \
    __asm__ volatile("lhu $12,%2(%0);"                                     \
                     "lhu $13,%3(%0);"                                     \
                     "lhu $14,%4(%0);"                                     \
                     "sh $12,0(%1);"                                       \
                     "sh $13,2(%1);"                                       \
                     "sh $14,4(%1)"                                        \
                     :                                                     \
                     : "r"(m), "r"(v), "i"(2 * (col)), "i"(6 + 2 * (col)), \
                       "i"(12 + 2 * (col))                                 \
                     : "$12", "$13", "$14", "memory")

/// Copies the SVECTOR `v` into column `col` (0-2) of `m`'s rotation
/// (`m->m[0][col] = v->vx` ... `m->m[2][col] = v->vz`) through `$12`-`$14`.
#define gte_WriteMatrixColumn(v, m, col)                                   \
    __asm__ volatile("lhu $12,0(%0);"                                      \
                     "lhu $13,2(%0);"                                      \
                     "lhu $14,4(%0);"                                      \
                     "sh $12,%2(%1);"                                      \
                     "sh $13,%3(%1);"                                      \
                     "sh $14,%4(%1)"                                       \
                     :                                                     \
                     : "r"(v), "r"(m), "i"(2 * (col)), "i"(6 + 2 * (col)), \
                       "i"(12 + 2 * (col))                                 \
                     : "$12", "$13", "$14", "memory")

/// The three MVMVA command words `gte_RotTransLV` issues, one per chunk of its input.
enum {
    GTE_ROT_TRANS_LV_LOW_ROTATE_TRANSLATE = 0x4A480012, /* Rotates the low chunks and adds translation.
                                                         *
                                                         * Same instruction word as gte_rtv0tr.
                                                         * MVMVA(sf=1, mx=0, v=0, cv=0, lm=0).
                                                         * V0 = (input & 1023) - 1024 * (input < 0),
                                                         * for each input component.
                                                         * Read MAC1..3 = (RT * V0 + TR * 4096) >> 12.
                                                         * Code reads MAC1..3, not saturated IR. */
    GTE_ROT_TRANS_LV_MIDDLE_ROTATE = 0x4A40E012,        /* Rotates signed middle chunks without translation.
                                                         *
                                                         * gte_rtv1 operands with the fraction shift off.
                                                         * MVMVA(sf=0, mx=0, v=1, cv=3, lm=0).
                                                         * V1 = ((input >> 10) & 1023) - 1023 * (input < 0),
                                                         * for each input component.
                                                         * Read MAC1..3 = RT * V1; IR1..3 saturate.
                                                         * Arithmetic MAC >> 2 restores the 2^10 weight
                                                         * against RT's 12 fractional bits. */
    GTE_ROT_TRANS_LV_HIGH_ROTATE = 0x4A416012           /* Rotates signed high chunks without translation.
                                                         *
                                                         * MVMVA(sf=0, mx=0, v=2, cv=3, lm=0).
                                                         * V2 = (input >> 20) + (input < 0), componentwise.
                                                         * Read MAC1..3 = RT * V2; IR1..3 saturate.
                                                         * MAC << 8 restores the 2^20 chunk's weight
                                                         * with RT's 12 fractional bits; wraps at 32 bits. */
};

/// Rotates a signed 32-bit vector and adds the translation already loaded into the GTE.
///
/// Load RT (12 fractional bits) and TR before calling. `inputVector` and
/// `outputVector` each address three contiguous signed 32-bit components,
/// aligned to four bytes; input and TR use the same coordinate units. Reads
/// and writes exactly 12 bytes. The buffers may overlap: all input loads
/// precede the output stores. Each pointer expression is evaluated once,
/// with no ordering between the two evaluations and no captured C variables.
///
/// Splits each component into signed low, middle and high chunks weighted by
/// 1, 2^10 and 2^20. The low product includes TR and shifts right by 12; the
/// middle product shifts right by 2; the high product shifts left by 8.
/// Low and middle products round down separately, so this can differ from
/// shifting a single full product. Final additions wrap at 32 bits.
/// Clobbers GTE V0..V2, MAC1..3, IR1..3 and FLAG; leaves RT and TR intact.
/// No combined overflow status is returned.
#define gte_RotTransLV(inputVector, outputVector) \
    __asm__ volatile( \
        "lw	$14, 0( %0 );" \
        "lw	$15, 4( %0 );" \
        "addiu	$16, $0, -0x400;" \
        "sra	$12, $14, 21;" \
        "and	$12, $16, $12;" \
        "andi	$13, $14, 0x3ff;" \
        "or	$12, $13, $12;" \
        "andi	$12, $12, 0xffff;" \
        "sra	$13, $15, 21;" \
        "and	$13, $16, $13;" \
        "andi	$16, $15, 0x3ff;" \
        "or	$13, $16, $13;" \
        "sll	$13, $13, 16;" \
        "or	$12, $13, $12;" \
        "mtc2	$12, $0;" \
        "sra	$14, $14, 10;" \
        "sra	$15, $15, 10;" \
        "addiu	$16, $0, -0x400;" \
        "sra	$12, $14, 21;" \
        "and	$12, $16, $12;" \
        "andi	$13, $14, 0x3ff;" \
        "or	$12, $13, $12;" \
        "sra	$13, $15, 21;" \
        "and	$13, $16, $13;" \
        "andi	$16, $15, 0x3ff;" \
        "or	$13, $16, $13;" \
        "srl	$16, $15, 31;" \
        "addu	$13, $13, $16;" \
        "sll	$13, $13, 16;" \
        "srl	$16, $14, 31;" \
        "addu	$12, $12, $16;" \
        "andi	$12, $12, 0xffff;" \
        "or	$12, $13, $12;" \
        "mtc2	$12, $2;" \
        "sra	$14, $14, 10;" \
        "sra	$15, $15, 10;" \
        "andi	$12, $14, 0xffff;" \
        "srl	$16, $14, 31;" \
        "addu	$12, $16, $12;" \
        "andi	$12, $12, 0xffff;" \
        "andi	$13, $15, 0xffff;" \
        "srl	$16, $15, 31;" \
        "addu	$13, $16, $13;" \
        "sll	$13, $13, 16;" \
        "or	$12, $13, $12;" \
        "mtc2	$12, $4;" \
        "lw	$16, 8( %0 );" \
        "addiu	$14, $0, -0x400;" \
        "srl	$15, $16, 31;" \
        "sra	$12, $16, 21;" \
        "and	$12, $14, $12;" \
        "andi	$13, $16, 0x3ff;" \
        "or	$12, $13, $12;" \
        "mtc2	$12, $1;" \
        "sra	$16, $16, 10;" \
        "sra	$12, $16, 21;" \
        "and	$12, $14, $12;" \
        "andi	$13, $16, 0x3ff;" \
        "or	$12, $13, $12;" \
        "addu	$12, $12, $15;" \
        "mtc2	$12, $3;" \
        "sra	$16, $16, 10;" \
        "addu	$12, $16, $15;" \
        "mtc2	$12, $5;" \
        "nop;" \
        "nop;" /* Use MAC results so IR saturation does not clamp the accumulated output. */ \
        ".word %2;" \
        "mfc2	$14, $25;" \
        "mfc2	$15, $26;" \
        "mfc2	$16, $27;" \
        "nop;" \
        "nop;" \
        ".word %3;" \
        "mfc2	$12, $25;" \
        "nop;" \
        "sra	$12, $12, 2;" \
        "addu	$14, $12, $14;" \
        "mfc2	$12, $26;" \
        "nop;" \
        "sra	$12, $12, 2;" \
        "addu	$15, $12, $15;" \
        "mfc2	$12, $27;" \
        "nop;" \
        "sra	$12, $12, 2;" \
        "addu	$16, $12, $16;" \
        "nop;" \
        "nop;" \
        ".word %4;" \
        "mfc2	$12, $25;" \
        "nop;" \
        "sll	$12, $12, 8;" \
        "addu	$14, $12, $14;" \
        "mfc2	$12, $26;" \
        "nop;" \
        "sll	$12, $12, 8;" \
        "addu	$15, $12, $15;" \
        "mfc2	$12, $27;" \
        "nop;" \
        "sll	$12, $12, 8;" \
        "addu	$16, $12, $16;" \
        "sw	$14, 0( %1 );" \
        "sw	$15, 4( %1 );" \
        "sw	$16, 8( %1 )" \
        : \
        : "r"(inputVector), "r"(outputVector), \
        "i"(GTE_ROT_TRANS_LV_LOW_ROTATE_TRANSLATE), \
        "i"(GTE_ROT_TRANS_LV_MIDDLE_ROTATE), \
        "i"(GTE_ROT_TRANS_LV_HIGH_ROTATE) \
        : "$12", "$13", "$14", "$15", "$16", "memory")

#endif
