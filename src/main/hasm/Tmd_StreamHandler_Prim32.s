.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_StreamHandler_Prim32  (VRAM 0x800105cc / ROM 0xdcc)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * The draw pass's handler for a stream's pre-transformed untextured gouraud
 * triangle records: one body with a second entry, the two differing only in the
 * packet code byte they stamp. Prim32 stamps the blended form (0x32), the alabel
 * tmdDrawStreamPrimG3PreXform the opaque one (0x30).
 * _tmdResolveSourceDrawHandlers resolves an entry into a stream beside its opcode and
 * Tmd_DispatchStream jalr's it (documented in include/main/tmd.h).
 * Early-image placement (linker_section_order: .rodata).
 */

.section .text, "ax"

glabel Tmd_StreamHandler_Prim32
    /* DCC 800105CC */  lw          $t9, 0x18($a0)
    /* DD0 800105D0 */  lw          $a3, 0x1C($a0)
    /* DD4 800105D4 */  lw          $t8, 0x4($a0)
    /* DD8 800105D8 */  lw          $t7, 0x14($a0)
    /* DDC 800105DC */  lw          $t6, 0x10($a0)
    /* DE0 800105E0 */  sll         $t9, $t9, 2
    /* DE4 800105E4 */  lui         $v0, 0x600
    /* DE8 800105E8 */  addiu       $v1, $zero, 0x32
    /* DEC 800105EC */  j           .LtmdG3PreXformLoop
    /* DF0 800105F0 */  lw          $a1, 0x84($a0)

/* POLY_G3 includes its DMA tag; the GPU length excludes that word. */
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_PACKET_BYTES, 28
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_PACKET_WORDS, (TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_PACKET_BYTES / 4) - 1
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_OPAQUE_CODE, 0x30
/* Wrap scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DEPTH_SHIFT, 4
/* GPU linked-list addresses retain the low 24 address bits. */
.equ TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DMA_ADDRESS_MASK, 0x00FFFFFF

/*
 * Complete the packet's command/tag and prepend it to its depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 OTZ, a1 depth shift, t0 DMA address mask, v1 GPU command byte,
 * v0 six-word DMA length in bits 24..31. Clobbers t2 and t5; writes the
 * OT head, packet tag and command byte, retaining all XY and RGB words.
 * Fixed registers and constants belong to this body's two entries. Expansion
 * emits no call or extra delay instructions and is undefined after the body.
 */
.macro TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_LINK_PACKET
    sb          $v1, 0x7($t8)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t5, 0x0($t2)
    sw          $t1, 0x0($t2)
    and         $t5, $t0, $t5
    or          $t5, $v0, $t5
    sw          $t5, 0x0($t8)
.endm

alabel tmdDrawStreamPrimG3PreXform
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* t8 first-region packet cursor, t6 depth cache, t7 displaced OT base. */
    /* DF4 800105F4 */  lw          $t9, 0x18($a0)
    /* DF8 800105F8 */  lw          $a3, 0x1C($a0)
    /* DFC 800105FC */  lw          $t8, 0x4($a0)
    /* E00 80010600 */  lw          $t7, 0x14($a0)
    /* E04 80010604 */  lw          $t6, 0x10($a0)
    /* E08 80010608 */  sll         $t9, $t9, 2
    /* E0C 8001060C */  lui         $v0, (TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_PACKET_WORDS << 8)
    /* E10 80010610 */  addiu       $v1, $zero, TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_OPAQUE_CODE
    /* E14 80010614 */  j           .LtmdG3PreXformLoop
    /* E18 80010618 */  lw          $a1, 0x84($a0)
  .LtmdG3PreXformAdvance:
    /* Rejection consumes the same slot as an accepted triangle. */
    /* E1C 8001061C */  addiu       $t8, $t8, TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_PACKET_BYTES
    /* E20 80010620 */  addu        $a2, $t9, $a2
  .LtmdG3PreXformLoop:
    /* E24 80010624 */  beq         $zero, $a3, .LtmdG3PreXformReturn
    /* E28 80010628 */  nop
    /* Feed the packet's existing corners through SXY's FIFO for the facing test. */
    /* E2C 8001062C */  lwc2        $15, 0x8($t8)
    /* E30 80010630 */  lwc2        $15, 0x10($t8)
    /* E34 80010634 */  lwc2        $15, 0x18($t8)
    /* E38 80010638 */  addiu       $a3, $a3, -0x1
    /* E3C 8001063C */  nop
    /* E40 80010640 */  nclip
    /* E44 80010644 */  lw          $t1, 0x0($a2)
    /* E48 80010648 */  lw          $t3, 0x4($a2)
    /* E4C 8001064C */  mfc2        $t0, $24
    /* E50 80010650 */  srl         $t2, $t1, 16
    /* Reject nonpositive winding, then failed projections marked with TMD_VERTEX_DEPTH_INVALID. */
    /* E54 80010654 */  blez        $t0, .LtmdG3PreXformAdvance
    /* E58 80010658 */  addu        $t2, $t6, $t2
    /* E5C 8001065C */  lw          $t2, 0x0($t2)
    /* E60 80010660 */  sll         $t1, $t1, 16
    /* E64 80010664 */  bltz        $t2, .LtmdG3PreXformAdvance
    /* E68 80010668 */  srl         $t1, $t1, 16
    /* E6C 8001066C */  addu        $t1, $t6, $t1
    /* E70 80010670 */  lw          $t1, 0x0($t1)
    /* E74 80010674 */  sll         $t3, $t3, 16
    /* E78 80010678 */  bltz        $t1, .LtmdG3PreXformAdvance
    /* E7C 8001067C */  srl         $t3, $t3, 16
    /* E80 80010680 */  addu        $t3, $t6, $t3
    /* E84 80010684 */  lw          $t3, 0x0($t3)
    /* E88 80010688 */  mtc2        $t1, $17
    /* E8C 8001068C */  bltz        $t3, .LtmdG3PreXformAdvance
    /* E90 80010690 */  mtc2        $t2, $18
    /* E94 80010694 */  mtc2        $t3, $19
    /* Average valid cached depths; the following address mask encodes GPU DMA links. */
    /* E98 80010698 */  lui         $t0, (TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* E9C 8001069C */  ori         $t0, $t0, (TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* EA0 800106A0 */  avsz3
    /* EA4 800106A4 */  and         $t1, $t0, $t8
    /* EA8 800106A8 */  mfc2        $t2, $7
    /* EAC..ED4 800106AC..800106D4 */  TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_LINK_PACKET
    /* ED8 800106D8 */  j           .LtmdG3PreXformAdvance
    /* EDC 800106DC */  nop
  .LtmdG3PreXformReturn:
    /* EE0 800106E0 */  sw          $t8, 0x4($a0)
    /* EE4 800106E4 */  addu        $v0, $zero, $a2
    /* EE8 800106E8 */  jr          $ra
    /* EEC 800106EC */  nop
endlabel Tmd_StreamHandler_Prim32
.purgem TMD_DRAW_STREAM_PRIM_G3_PRE_XFORM_LINK_PACKET
