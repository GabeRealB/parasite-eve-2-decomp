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
/// the light colour matrix LCM, the background colour BK and the far colour FC.
/// Results land in MAC1..3 and IR1..3 (and MAC0 for scalar results); colours
/// are pushed into the colour FIFO RGB0..2, screen points into SXY0..2 and
/// depths into SZ0..3. Products are shifted right by 12 bits for 4.12 fixed
/// point; the `0` and `_sf0` forms leave them unshifted. Commands marked
/// "clamped" limit IR1..3 to non-negative values.

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
/// name spells the operands. Matrix: `rt` the rotation matrix, `ll` the light
/// matrix, `lc` the light colour matrix. Vector: `v0`, `v1`, `v2` or `ir`.
/// Offset: none, `tr` the translation vector, `bk` the background colour.
/// `gte_rt` is `gte_rtv0tr` under Psy-Q's rotate-and-translate name, and
/// `gte_rtir_sf0` is `gte_rtir` without the 12-bit shift. `gte_ll` (LLM·V0) and
/// `gte_lc` (BK + LCM·IR) are clamped: they are the light and colour steps of
/// vertex lighting.
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
#undef gte_rtir_sf0
#define gte_rtir_sf0() __asm__ volatile("nop; nop; .word 0x4A41E012")
#undef gte_rtv0tr
#define gte_rtv0tr()   __asm__ volatile("nop; nop; .word 0x4A480012")
#undef gte_rtv1tr
#define gte_rtv1tr()   __asm__ volatile("nop; nop; .word 0x4A488012")
#undef gte_rtv2tr
#define gte_rtv2tr()   __asm__ volatile("nop; nop; .word 0x4A490012")
#undef gte_rtirtr
#define gte_rtirtr()   __asm__ volatile("nop; nop; .word 0x4A498012")
#undef gte_rtv0bk
#define gte_rtv0bk()   __asm__ volatile("nop; nop; .word 0x4A482012")
#undef gte_rtv1bk
#define gte_rtv1bk()   __asm__ volatile("nop; nop; .word 0x4A48A012")
#undef gte_rtv2bk
#define gte_rtv2bk()   __asm__ volatile("nop; nop; .word 0x4A492012")
#undef gte_rtirbk
#define gte_rtirbk()   __asm__ volatile("nop; nop; .word 0x4A49A012")
#undef gte_ll
#define gte_ll()       __asm__ volatile("nop; nop; .word 0x4A4A6412")
#undef gte_llv0
#define gte_llv0()     __asm__ volatile("nop; nop; .word 0x4A4A6012")
#undef gte_llv1
#define gte_llv1()     __asm__ volatile("nop; nop; .word 0x4A4AE012")
#undef gte_llv2
#define gte_llv2()     __asm__ volatile("nop; nop; .word 0x4A4B6012")
#undef gte_llir
#define gte_llir()     __asm__ volatile("nop; nop; .word 0x4A4BE012")
#undef gte_llv0tr
#define gte_llv0tr()   __asm__ volatile("nop; nop; .word 0x4A4A0012")
#undef gte_llv1tr
#define gte_llv1tr()   __asm__ volatile("nop; nop; .word 0x4A4A8012")
#undef gte_llv2tr
#define gte_llv2tr()   __asm__ volatile("nop; nop; .word 0x4A4B0012")
#undef gte_llirtr
#define gte_llirtr()   __asm__ volatile("nop; nop; .word 0x4A4B8012")
#undef gte_llv0bk
#define gte_llv0bk()   __asm__ volatile("nop; nop; .word 0x4A4A2012")
#undef gte_llv1bk
#define gte_llv1bk()   __asm__ volatile("nop; nop; .word 0x4A4AA012")
#undef gte_llv2bk
#define gte_llv2bk()   __asm__ volatile("nop; nop; .word 0x4A4B2012")
#undef gte_llirbk
#define gte_llirbk()   __asm__ volatile("nop; nop; .word 0x4A4BA012")
#undef gte_lc
#define gte_lc()       __asm__ volatile("nop; nop; .word 0x4A4DA412")
#undef gte_lcv0
#define gte_lcv0()     __asm__ volatile("nop; nop; .word 0x4A4C6012")
#undef gte_lcv1
#define gte_lcv1()     __asm__ volatile("nop; nop; .word 0x4A4CE012")
#undef gte_lcv2
#define gte_lcv2()     __asm__ volatile("nop; nop; .word 0x4A4D6012")
#undef gte_lcir
#define gte_lcir()     __asm__ volatile("nop; nop; .word 0x4A4DE012")
#undef gte_lcv0tr
#define gte_lcv0tr()   __asm__ volatile("nop; nop; .word 0x4A4C0012")
#undef gte_lcv1tr
#define gte_lcv1tr()   __asm__ volatile("nop; nop; .word 0x4A4C8012")
#undef gte_lcv2tr
#define gte_lcv2tr()   __asm__ volatile("nop; nop; .word 0x4A4D0012")
#undef gte_lcirtr
#define gte_lcirtr()   __asm__ volatile("nop; nop; .word 0x4A4D8012")
#undef gte_lcv0bk
#define gte_lcv0bk()   __asm__ volatile("nop; nop; .word 0x4A4C2012")
#undef gte_lcv1bk
#define gte_lcv1bk()   __asm__ volatile("nop; nop; .word 0x4A4CA012")
#undef gte_lcv2bk
#define gte_lcv2bk()   __asm__ volatile("nop; nop; .word 0x4A4D2012")
#undef gte_lcirbk
#define gte_lcirbk()   __asm__ volatile("nop; nop; .word 0x4A4DA012")
/// Depth cueing: blend a colour toward the far colour FC by IR0 and push the
/// result into the colour FIFO. `gte_dpcs` blends the input colour RGBC,
/// `gte_dpct` each of RGB0..2, and `gte_dpcl` the input colour after scaling it
/// by IR1..3. `gte_intpl` blends the IR vector itself.
#undef gte_dpcl
#define gte_dpcl()     __asm__ volatile("nop; nop; .word 0x4A680029")
#undef gte_dpcs
#define gte_dpcs()     __asm__ volatile("nop; nop; .word 0x4A780010")
#undef gte_dpct
#define gte_dpct()     __asm__ volatile("nop; nop; .word 0x4AF8002A")
#undef gte_intpl
#define gte_intpl()    __asm__ volatile("nop; nop; .word 0x4A980011")
/// IR1..3 squared component by component, clamped.
#undef gte_sqr12
#define gte_sqr12()    __asm__ volatile("nop; nop; .word 0x4AA80428")
#undef gte_sqr0
#define gte_sqr0()     __asm__ volatile("nop; nop; .word 0x4AA00428")
/// Lighting, clamped, results pushed into the colour FIFO. The `nc` commands
/// light V0 (`s`, single) or V0..V2 (`t`, triple) as a surface normal: the
/// light step LLM·V then the colour step BK + LCM·IR. `nc` stops there, `ncc`
/// also multiplies by the input colour RGBC, and `ncd` does that and then
/// depth-cues toward FC. `gte_cc` and `gte_cdp` run the colour step on IR
/// without a normal: `gte_cc` multiplies by RGBC, `gte_cdp` also depth-cues.
#undef gte_ncs
#define gte_ncs()      __asm__ volatile("nop; nop; .word 0x4AC8041E")
#undef gte_nct
#define gte_nct()      __asm__ volatile("nop; nop; .word 0x4AD80420")
#undef gte_ncds
#define gte_ncds()     __asm__ volatile("nop; nop; .word 0x4AE80413")
#undef gte_ncdt
#define gte_ncdt()     __asm__ volatile("nop; nop; .word 0x4AF80416")
#undef gte_nccs
#define gte_nccs()     __asm__ volatile("nop; nop; .word 0x4B08041B")
#undef gte_ncct
#define gte_ncct()     __asm__ volatile("nop; nop; .word 0x4B18043F")
#undef gte_cdp
#define gte_cdp()      __asm__ volatile("nop; nop; .word 0x4B280414")
#undef gte_cc
#define gte_cc()       __asm__ volatile("nop; nop; .word 0x4B38041C")
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
#undef gte_op0
#define gte_op0()      __asm__ volatile("nop; nop; .word 0x4B70000C")
/// General-purpose interpolation. `gte_gpf*` scales IR1..3 by IR0;
/// `gte_gpl*` adds that product to the current MAC1..3, so a sequence of them
/// accumulates a weighted sum. Both push the result into the colour FIFO.
#undef gte_gpf12
#define gte_gpf12()    __asm__ volatile("nop; nop; .word 0x4B98003D")
#undef gte_gpf0
#define gte_gpf0()     __asm__ volatile("nop; nop; .word 0x4B90003D")
#undef gte_gpl12
#define gte_gpl12()    __asm__ volatile("nop; nop; .word 0x4BA8003E")
#undef gte_gpl0
#define gte_gpl0()     __asm__ volatile("nop; nop; .word 0x4BA0003E")

/// The `_b` forms issue the same commands without the two leading `nop`s.
/// The `nop`s cover the delay the GTE needs between a register load and a
/// command reading it; Psy-Q offers these for code that has already spaced
/// its loads itself.
#undef gte_rtps_b
#define gte_rtps_b()     __asm__ volatile(".word 0x4A180001")
#undef gte_rtpt_b
#define gte_rtpt_b()     __asm__ volatile(".word 0x4A280030")
#undef gte_rt_b
#define gte_rt_b()       __asm__ volatile(".word 0x4A480012")
#undef gte_rtv0_b
#define gte_rtv0_b()     __asm__ volatile(".word 0x4A486012")
#undef gte_rtv1_b
#define gte_rtv1_b()     __asm__ volatile(".word 0x4A48E012")
#undef gte_rtv2_b
#define gte_rtv2_b()     __asm__ volatile(".word 0x4A496012")
#undef gte_rtir_b
#define gte_rtir_b()     __asm__ volatile(".word 0x4A49E012")
#undef gte_rtir_sf0_b
#define gte_rtir_sf0_b() __asm__ volatile(".word 0x4A41E012")
#undef gte_rtv0tr_b
#define gte_rtv0tr_b()   __asm__ volatile(".word 0x4A480012")
#undef gte_rtv1tr_b
#define gte_rtv1tr_b()   __asm__ volatile(".word 0x4A488012")
#undef gte_rtv2tr_b
#define gte_rtv2tr_b()   __asm__ volatile(".word 0x4A490012")
#undef gte_rtirtr_b
#define gte_rtirtr_b()   __asm__ volatile(".word 0x4A498012")
#undef gte_rtv0bk_b
#define gte_rtv0bk_b()   __asm__ volatile(".word 0x4A482012")
#undef gte_rtv1bk_b
#define gte_rtv1bk_b()   __asm__ volatile(".word 0x4A48A012")
#undef gte_rtv2bk_b
#define gte_rtv2bk_b()   __asm__ volatile(".word 0x4A492012")
#undef gte_rtirbk_b
#define gte_rtirbk_b()   __asm__ volatile(".word 0x4A49A012")
#undef gte_ll_b
#define gte_ll_b()       __asm__ volatile(".word 0x4A4A6412")
#undef gte_llv0_b
#define gte_llv0_b()     __asm__ volatile(".word 0x4A4A6012")
#undef gte_llv1_b
#define gte_llv1_b()     __asm__ volatile(".word 0x4A4AE012")
#undef gte_llv2_b
#define gte_llv2_b()     __asm__ volatile(".word 0x4A4B6012")
#undef gte_llir_b
#define gte_llir_b()     __asm__ volatile(".word 0x4A4BE012")
#undef gte_llv0tr_b
#define gte_llv0tr_b()   __asm__ volatile(".word 0x4A4A0012")
#undef gte_llv1tr_b
#define gte_llv1tr_b()   __asm__ volatile(".word 0x4A4A8012")
#undef gte_llv2tr_b
#define gte_llv2tr_b()   __asm__ volatile(".word 0x4A4B0012")
#undef gte_llirtr_b
#define gte_llirtr_b()   __asm__ volatile(".word 0x4A4B8012")
#undef gte_llv0bk_b
#define gte_llv0bk_b()   __asm__ volatile(".word 0x4A4A2012")
#undef gte_llv1bk_b
#define gte_llv1bk_b()   __asm__ volatile(".word 0x4A4AA012")
#undef gte_llv2bk_b
#define gte_llv2bk_b()   __asm__ volatile(".word 0x4A4B2012")
#undef gte_llirbk_b
#define gte_llirbk_b()   __asm__ volatile(".word 0x4A4BA012")
#undef gte_lc_b
#define gte_lc_b()       __asm__ volatile(".word 0x4A4DA412")
#undef gte_lcv0_b
#define gte_lcv0_b()     __asm__ volatile(".word 0x4A4C6012")
#undef gte_lcv1_b
#define gte_lcv1_b()     __asm__ volatile(".word 0x4A4CE012")
#undef gte_lcv2_b
#define gte_lcv2_b()     __asm__ volatile(".word 0x4A4D6012")
#undef gte_lcir_b
#define gte_lcir_b()     __asm__ volatile(".word 0x4A4DE012")
#undef gte_lcv0tr_b
#define gte_lcv0tr_b()   __asm__ volatile(".word 0x4A4C0012")
#undef gte_lcv1tr_b
#define gte_lcv1tr_b()   __asm__ volatile(".word 0x4A4C8012")
#undef gte_lcv2tr_b
#define gte_lcv2tr_b()   __asm__ volatile(".word 0x4A4D0012")
#undef gte_lcirtr_b
#define gte_lcirtr_b()   __asm__ volatile(".word 0x4A4D8012")
#undef gte_lcv0bk_b
#define gte_lcv0bk_b()   __asm__ volatile(".word 0x4A4C2012")
#undef gte_lcv1bk_b
#define gte_lcv1bk_b()   __asm__ volatile(".word 0x4A4CA012")
#undef gte_lcv2bk_b
#define gte_lcv2bk_b()   __asm__ volatile(".word 0x4A4D2012")
#undef gte_lcirbk_b
#define gte_lcirbk_b()   __asm__ volatile(".word 0x4A4DA012")
#undef gte_dpcl_b
#define gte_dpcl_b()     __asm__ volatile(".word 0x4A680029")
#undef gte_dpcs_b
#define gte_dpcs_b()     __asm__ volatile(".word 0x4A780010")
#undef gte_dpct_b
#define gte_dpct_b()     __asm__ volatile(".word 0x4AF8002A")
#undef gte_intpl_b
#define gte_intpl_b()    __asm__ volatile(".word 0x4A980011")
#undef gte_sqr12_b
#define gte_sqr12_b()    __asm__ volatile(".word 0x4AA80428")
#undef gte_sqr0_b
#define gte_sqr0_b()     __asm__ volatile(".word 0x4AA00428")
#undef gte_ncs_b
#define gte_ncs_b()      __asm__ volatile(".word 0x4AC8041E")
#undef gte_nct_b
#define gte_nct_b()      __asm__ volatile(".word 0x4AD80420")
#undef gte_ncds_b
#define gte_ncds_b()     __asm__ volatile(".word 0x4AE80413")
#undef gte_ncdt_b
#define gte_ncdt_b()     __asm__ volatile(".word 0x4AF80416")
#undef gte_nccs_b
#define gte_nccs_b()     __asm__ volatile(".word 0x4B08041B")
#undef gte_ncct_b
#define gte_ncct_b()     __asm__ volatile(".word 0x4B18043F")
#undef gte_cdp_b
#define gte_cdp_b()      __asm__ volatile(".word 0x4B280414")
#undef gte_cc_b
#define gte_cc_b()       __asm__ volatile(".word 0x4B38041C")
#undef gte_nclip_b
#define gte_nclip_b()    __asm__ volatile(".word 0x4B400006")
#undef gte_avsz3_b
#define gte_avsz3_b()    __asm__ volatile(".word 0x4B58002D")
#undef gte_avsz4_b
#define gte_avsz4_b()    __asm__ volatile(".word 0x4B68002E")
#undef gte_op12_b
#define gte_op12_b()     __asm__ volatile(".word 0x4B78000C")
#undef gte_op0_b
#define gte_op0_b()      __asm__ volatile(".word 0x4B70000C")
#undef gte_gpf12_b
#define gte_gpf12_b()    __asm__ volatile(".word 0x4B98003D")
#undef gte_gpf0_b
#define gte_gpf0_b()     __asm__ volatile(".word 0x4B90003D")
#undef gte_gpl12_b
#define gte_gpl12_b()    __asm__ volatile(".word 0x4BA8003E")
#undef gte_gpl0_b
#define gte_gpl0_b()     __asm__ volatile(".word 0x4BA0003E")

/// Rotations without the 12-bit fraction shift (`sf` = 0), which Psy-Q names
/// only for the IR vector (`gte_rtir_sf0`): the product of the rotation matrix
/// and V0, V1 or V2, with no translation added.
#define gte_rtv0_sf0() __asm__ volatile("nop; nop; .word 0x4A406012")

/// Loads one register of the GTE's screen-Z FIFO (SZ0-SZ3). Psy-Q loads these
/// only as a group (`gte_ldsz3`, `gte_ldsz4`); code that tests each depth
/// before loading it uses the single loads.
#define gte_ldSZ0(r0) __asm__ volatile("mtc2 %0, $16" : : "r"(r0))
#define gte_ldSZ1(r0) __asm__ volatile("mtc2 %0, $17" : : "r"(r0))
#define gte_ldSZ2(r0) __asm__ volatile("mtc2 %0, $18" : : "r"(r0))
#define gte_ldSZ3(r0) __asm__ volatile("mtc2 %0, $19" : : "r"(r0))

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

#endif
