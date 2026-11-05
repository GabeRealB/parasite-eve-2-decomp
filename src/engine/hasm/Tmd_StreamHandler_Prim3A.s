.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_StreamHandler_Prim3A  (VRAM 0x800106f0 / ROM 0xef0)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * TMD stream handler (function pointer from _tmdResolveSourceDrawHandlers).
 * Early-image placement (linker_section_order: .rodata).
 *
 * Two entries onto one body, one per primitive code the body stamps: the
 * opaque quad, tmdDrawStreamPrimG4PreXform (0x38), and the blended one,
 * Tmd_StreamHandler_Prim3A (0x3A). _tmdResolveSourceDrawHandlers resolves the opaque
 * entry for records 0x61/0x161; no opcode resolves the blended entry, and
 * nothing else in the image references it either.
 */

.section .text, "ax"

glabel Tmd_StreamHandler_Prim3A
    /* EF0 800106F0 */  lw          $t9, 0x18($a0)
    /* EF4 800106F4 */  lw          $a3, 0x1C($a0)
    /* EF8 800106F8 */  lw          $t8, 0x4($a0)
    /* EFC 800106FC */  lw          $t7, 0x14($a0)
    /* F00 80010700 */  lw          $t6, 0x10($a0)
    /* F04 80010704 */  sll         $t9, $t9, 2
    /* F08 80010708 */  lui         $v0, 0x800
    /* F0C 8001070C */  addiu       $v1, $zero, 0x3A
    /* F10 80010710 */  j           .LtmdG4PreXformLoop
    /* F14 80010714 */  lw          $a1, 0x84($a0)

/* POLY_G4 includes its DMA tag; the GPU length excludes that word. */
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_PACKET_BYTES, 36
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_PACKET_WORDS, (TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_PACKET_BYTES / 4) - 1
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_OPAQUE_CODE, 0x38
/* Wrap scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DEPTH_SHIFT, 4
/* GPU linked-list addresses retain the low 24 address bits. */
.equ TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DMA_ADDRESS_MASK, 0x00FFFFFF

/*
 * Complete the packet's command/tag and prepend it to its depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 OTZ, a1 depth shift, t0 DMA address mask, v1 GPU command byte,
 * v0 eight-word DMA length in bits 24..31. Clobbers t2 and t5; writes the
 * OT head, packet tag and command byte, retaining all XY and RGB words.
 * Fixed registers and constants belong to this body's two entries. Expansion
 * emits no call or extra delay instructions and is undefined after the body.
 */
.macro TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_LINK_PACKET
    sb          $v1, 0x7($t8)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t5, 0x0($t2)
    sw          $t1, 0x0($t2)
    and         $t5, $t0, $t5
    or          $t5, $v0, $t5
    sw          $t5, 0x0($t8)
.endm

alabel tmdDrawStreamPrimG4PreXform
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* t8 first-region packet cursor, t6 depth cache, t7 displaced OT base. */
    /* F18 80010718 */  lw          $t9, 0x18($a0)
    /* F1C 8001071C */  lw          $a3, 0x1C($a0)
    /* F20 80010720 */  lw          $t8, 0x4($a0)
    /* F24 80010724 */  lw          $t7, 0x14($a0)
    /* F28 80010728 */  lw          $t6, 0x10($a0)
    /* F2C 8001072C */  sll         $t9, $t9, 2
    /* F30 80010730 */  lui         $v0, (TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_PACKET_WORDS << 8)
    /* F34 80010734 */  addiu       $v1, $zero, TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_OPAQUE_CODE
    /* F38 80010738 */  j           .LtmdG4PreXformLoop
    /* F3C 8001073C */  lw          $a1, 0x84($a0)
  .LtmdG4PreXformAdvance:
    /* Rejection consumes the same slot as an accepted quad. */
    /* F40 80010740 */  addiu       $t8, $t8, TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_PACKET_BYTES
    /* F44 80010744 */  addu        $a2, $t9, $a2
  .LtmdG4PreXformLoop:
    /* F48 80010748 */  beq         $zero, $a3, .LtmdG4PreXformReturn
    /* F4C 8001074C */  nop
    /* Test the existing packet corners; the SXY FIFO first holds 0, 1, 2. */
    /* F50 80010750 */  lwc2        $15, 0x8($t8)
    /* F54 80010754 */  lwc2        $15, 0x10($t8)
    /* F58 80010758 */  lwc2        $15, 0x18($t8)
    /* F5C 8001075C */  addiu       $a3, $a3, -0x1
    /* F60 80010760 */  nop
    /* F64 80010764 */  nclip
    /* F68 80010768 */  lw          $t1, 0x0($a2)
    /* F6C 8001076C */  lw          $t3, 0x4($a2)
    /* F70 80010770 */  mfc2        $t0, $24
    /* F74 80010774 */  srl         $t2, $t1, 16
    /* F78 80010778 */  bgtz        $t0, .LtmdG4PreXformCheckDepths
    /* F7C 8001077C */  nop
    /* The fallback accepts negative winding of 1, 2, 3, not positive. */
    /* F80 80010780 */  lwc2        $15, 0x20($t8)
    /* F84 80010784 */  nop
    /* F88 80010788 */  nop
    /* F8C 8001078C */  nclip
    /* F90 80010790 */  mfc2        $t0, $24
    /* F94 80010794 */  nop
    /* F98 80010798 */  bgez        $t0, .LtmdG4PreXformAdvance
  .LtmdG4PreXformCheckDepths:
    /* Each halfword is a depth-cache byte offset; negative depths mark failed projections. */
    /* F9C 8001079C */  addu        $t2, $t6, $t2
    /* FA0 800107A0 */  lw          $t2, 0x0($t2)
    /* FA4 800107A4 */  sll         $t1, $t1, 16
    /* FA8 800107A8 */  bltz        $t2, .LtmdG4PreXformAdvance
    /* FAC 800107AC */  srl         $t1, $t1, 16
    /* FB0 800107B0 */  addu        $t1, $t6, $t1
    /* FB4 800107B4 */  lw          $t1, 0x0($t1)
    /* FB8 800107B8 */  srl         $t4, $t3, 16
    /* FBC 800107BC */  bltz        $t1, .LtmdG4PreXformAdvance
    /* FC0 800107C0 */  addu        $t4, $t6, $t4
    /* FC4 800107C4 */  lw          $t4, 0x0($t4)
    /* FC8 800107C8 */  sll         $t3, $t3, 16
    /* FCC 800107CC */  bltz        $t4, .LtmdG4PreXformAdvance
    /* FD0 800107D0 */  srl         $t3, $t3, 16
    /* FD4 800107D4 */  addu        $t3, $t6, $t3
    /* FD8 800107D8 */  lw          $t3, 0x0($t3)
    /* FDC 800107DC */  mtc2        $t1, $16
    /* FE0 800107E0 */  bltz        $t3, .LtmdG4PreXformAdvance
    /* FE4 800107E4 */  mtc2        $t2, $17
    /* FE8 800107E8 */  mtc2        $t3, $18
    /* FEC 800107EC */  mtc2        $t4, $19
    /* Average all four valid depths; the address mask encodes GPU DMA links. */
    /* FF0 800107F0 */  lui         $t0, (TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* FF4 800107F4 */  ori         $t0, $t0, (TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* FF8 800107F8 */  avsz4
    /* FFC 800107FC */  and         $t1, $t0, $t8
    /* 1000 80010800 */  mfc2        $t2, $7
    /* 1004..102C 80010804..8001082C */  TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_LINK_PACKET
    /* 1030 80010830 */  j           .LtmdG4PreXformAdvance
    /* 1034 80010834 */  nop
  .LtmdG4PreXformReturn:
    /* 1038 80010838 */  sw          $t8, 0x4($a0)
    /* 103C 8001083C */  addu        $v0, $zero, $a2
    /* 1040 80010840 */  jr          $ra
    /* 1044 80010844 */  nop
endlabel Tmd_StreamHandler_Prim3A
.purgem TMD_DRAW_STREAM_PRIM_G4_PRE_XFORM_LINK_PACKET
