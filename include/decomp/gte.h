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

#endif
