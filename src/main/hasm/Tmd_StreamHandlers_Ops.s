.include "macro.inc"

.set noat
.set noreorder

/*
 * TMD early-image stream handlers  (VRAM 0x80010A90 / ROM 0x1290)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * One body per stream opcode: _tmdResolveSourceDrawHandlers resolves the body into the
 * stream beside the opcode, and Tmd_DispatchStream jalr's it. Each body is
 * labelled for the command it serves (documented in include/main/tmd.h).
 * Dual-entry alternates use alabel (e.g.
 * alabel tmdDrawStreamPrimGt3OneNormalSemiTrans shares the body of glabel
 * tmdDrawStreamPrimGt3OneNormal).
 * Early-image placement (linker_section_order: .rodata).
 *
 * tmdDrawStreamPrimG3CornerNormals  untextured triangle, one normal per corner
 * tmdDrawStreamPrimG3   the same triangle, one normal for the whole face
 * tmdDrawStreamPrimG4CornerNormals  untextured quad, one normal per corner
 * tmdDrawStreamPrimG4   the same quad, one normal for the whole face
 * tmdDrawStreamGt3SemiTrans/tmdDrawStreamGt3  gouraud textured triangle (+ ABR)
 * tmdDrawStreamGt4SemiTrans/tmdDrawStreamGt4  gouraud textured quad (semi-transparent alternate)
 * tmdDrawStreamPrimGt3PreXform/tmdDrawStreamPrimGt3PreXformSemiTrans,
 * tmdDrawStreamPrimGt4PreXform/tmdDrawStreamPrimGt4PreXformSemiTrans
 *                        pre-transformed textured gouraud (+ ABR) tri/quad
 * tmdDrawStreamPrimGt3OneNormal/SemiTrans (0x18/0x1A)  one-normal textured tri, fixed colour
 * tmdDrawStreamPrimGt4OneNormal/SemiTrans (0x58/0x5A)  one-normal textured quad, fixed colour
 * tmdXformStreamVertsElemColor/tmdXformStreamVerts  transform pre-pass, element colour/fixed
 * tmdDrawStreamPrimGt3CornerColors  textured triangle, normal and material colour per corner
 * tmdDrawStreamPrimGt4CornerColors  textured quad, normal and material colour per corner
 */

.section .text, "ax"

/* POLY_G3 includes its DMA tag; the GPU length excludes that word. */
.equ TMD_DRAW_STREAM_G3_CORNER_NORMALS_PACKET_BYTES, 28
.equ TMD_DRAW_STREAM_G3_CORNER_NORMALS_PACKET_WORDS, (TMD_DRAW_STREAM_G3_CORNER_NORMALS_PACKET_BYTES / 4) - 1
/* Wrap scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_G3_CORNER_NORMALS_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_G3_CORNER_NORMALS_DEPTH_SHIFT, 4
.equ TMD_DRAW_STREAM_G3_CORNER_NORMALS_DMA_ADDRESS_BITS, 24

/*
 * Complete and prepend a G3 packet, keeping NCCT's colour result per corner.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 OTZ, a1 depth shift, v0 six-word DMA length in bits 24..31,
 * GTE RGB0..2 from the preceding NCCT. Clobbers t0 and t2; writes the
 * OT head, packet tag and three colour words. The link arithmetic runs
 * while NCCT completes; keep the colour stores after it without extra delays.
 * Fixed registers and constants belong to this handler; expansion emits no call.
 */
.macro TMD_DRAW_STREAM_G3_CORNER_NORMALS_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_G3_CORNER_NORMALS_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_G3_CORNER_NORMALS_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_G3_CORNER_NORMALS_DMA_ADDRESS_BITS)
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_G3_CORNER_NORMALS_DMA_ADDRESS_BITS)
    or          $t0, $v0, $t0
    sw          $t0, 0x0($t8)
    swc2        $20, 0x4($t8)
    swc2        $21, 0xC($t8)
    swc2        $22, 0x14($t8)
.endm

glabel tmdDrawStreamPrimG3CornerNormals
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* 1290 80010A90 */  lw          $t9, 0x18($a0)
    /* 1294 80010A94 */  lw          $a3, 0x1C($a0)
    /* 1298 80010A98 */  lw          $t8, 0x0($a0)
    /* 129C 80010A9C */  lw          $t7, 0x14($a0)
    /* 12A0 80010AA0 */  lw          $t6, 0x8($a0)
    /* 12A4 80010AA4 */  lw          $t5, 0xC($a0)
    /* 12A8 80010AA8 */  sll         $t9, $t9, 2
    /* 12AC 80010AAC */  lui         $v0, (TMD_DRAW_STREAM_G3_CORNER_NORMALS_PACKET_WORDS << 8)
    /* 12B0 80010AB0 */  j           .LtmdG3CornerNormalsLoop
    /* 12B4 80010AB4 */  lw          $a1, 0x84($a0)
  .LtmdG3CornerNormalsAdvance:
    /* Rejection consumes the same packet slot as an accepted triangle. */
    /* 12B8 80010AB8 */  addiu       $t8, $t8, TMD_DRAW_STREAM_G3_CORNER_NORMALS_PACKET_BYTES
    /* 12BC 80010ABC */  addu        $a2, $t9, $a2
  .LtmdG3CornerNormalsLoop:
    /* 12C0 80010AC0 */  beq         $zero, $a3, .LtmdG3CornerNormalsDone
    /* 12C4 80010AC4 */  nop
    /* 12C8 80010AC8 */  addiu       $a3, $a3, -0x1
    /* Unpack three vertex and three normal byte offsets from the element. */
    /* 12CC 80010ACC */  lw          $t1, 0x0($a2)
    /* 12D0 80010AD0 */  lw          $t3, 0x4($a2)
    /* 12D4 80010AD4 */  srl         $t2, $t1, 16
    /* 12D8 80010AD8 */  addu        $t2, $t6, $t2
    /* 12DC 80010ADC */  sll         $t1, $t1, 16
    /* 12E0 80010AE0 */  srl         $t1, $t1, 16
    /* 12E4 80010AE4 */  addu        $t1, $t6, $t1
    /* 12E8 80010AE8 */  srl         $t4, $t3, 16
    /* 12EC 80010AEC */  sll         $t3, $t3, 16
    /* 12F0 80010AF0 */  srl         $t3, $t3, 16
    /* 12F4 80010AF4 */  addu        $t3, $t6, $t3
    /* 12F8 80010AF8 */  lwc2        $0, 0x0($t1)
    /* 12FC 80010AFC */  lwc2        $1, 0x4($t1)
    /* 1300 80010B00 */  lwc2        $2, 0x0($t2)
    /* 1304 80010B04 */  lwc2        $3, 0x4($t2)
    /* 1308 80010B08 */  lwc2        $4, 0x0($t3)
    /* 130C 80010B0C */  lwc2        $5, 0x4($t3)
    /* 1310 80010B10 */  lw          $t2, 0x8($a2)
    /* 1314 80010B14 */  sll         $t1, $t4, 0
    /* Project all corners; retain normal addresses while RTPT completes. */
    /* 1318 80010B18 */  rtpt
    /* 131C 80010B1C */  addu        $t1, $t5, $t1
    /* 1320 80010B20 */  srl         $t3, $t2, 16
    /* 1324 80010B24 */  addu        $t3, $t5, $t3
    /* 1328 80010B28 */  sll         $t2, $t2, 16
    /* 132C 80010B2C */  srl         $t2, $t2, 16
    /* 1330 80010B30 */  addu        $t2, $t5, $t2
    /* 1334 80010B34 */  cfc2        $t0, $31
    /* 1338 80010B38 */  nop
    /* Reject the signed GTE error summary before testing positive winding. */
    /* 133C 80010B3C */  bltz        $t0, .LtmdG3CornerNormalsAdvance
    /* 1340 80010B40 */  nop
    /* 1344 80010B44 */  nop
    /* 1348 80010B48 */  nop
    /* 134C 80010B4C */  nclip
    /* 1350 80010B50 */  mfc2        $t0, $24
    /* 1354 80010B54 */  nop
    /* 1358 80010B58 */  blez        $t0, .LtmdG3CornerNormalsAdvance
    /* 135C 80010B5C */  nop
    /* 1360 80010B60 */  swc2        $12, 0x8($t8)
    /* 1364 80010B64 */  swc2        $13, 0x10($t8)
    /* 1368 80010B68 */  swc2        $14, 0x18($t8)
    /* 136C 80010B6C */  nop
    /* 1370 80010B70 */  nop
    /* Average projected depths, then light the material colour with all three normals. */
    /* 1374 80010B74 */  avsz3
    /* 1378 80010B78 */  lwc2        $6, 0xC($a2)
    /* 137C 80010B7C */  lwc2        $0, 0x0($t1)
    /* 1380 80010B80 */  lwc2        $1, 0x4($t1)
    /* 1384 80010B84 */  lwc2        $2, 0x0($t2)
    /* 1388 80010B88 */  lwc2        $3, 0x4($t2)
    /* 138C 80010B8C */  lwc2        $4, 0x0($t3)
    /* 1390 80010B90 */  lwc2        $5, 0x4($t3)
    /* 1394 80010B94 */  mfc2        $t2, $7
    /* 1398 80010B98 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_G3_CORNER_NORMALS_DMA_ADDRESS_BITS)
    /* 139C 80010B9C */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_G3_CORNER_NORMALS_DMA_ADDRESS_BITS)
    /* 13A0 80010BA0 */  ncct
    /* 13A4..13D8 80010BA4..80010BD8 */  TMD_DRAW_STREAM_G3_CORNER_NORMALS_LINK_PACKET
    /* 13DC 80010BDC */  j           .LtmdG3CornerNormalsAdvance
    /* 13E0 80010BE0 */  nop
  .LtmdG3CornerNormalsDone:
    /* 13E4 80010BE4 */  sw          $t8, 0x0($a0)
    /* 13E8 80010BE8 */  addu        $v0, $zero, $a2
    /* 13EC 80010BEC */  jr          $ra
    /* 13F0 80010BF0 */  nop
endlabel tmdDrawStreamPrimG3CornerNormals
.purgem TMD_DRAW_STREAM_G3_CORNER_NORMALS_LINK_PACKET

/* POLY_G4 includes its DMA tag; the GPU length excludes that word. */
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_PACKET_BYTES, 36
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_PACKET_WORDS, (TMD_DRAW_STREAM_G4_CORNER_NORMALS_PACKET_BYTES / 4) - 1
/* Wrap scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_DEPTH_SHIFT, 4
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_DMA_ADDRESS_BITS, 24
/* Outside the unsigned u16 vertex-offset domain; forces a fresh projection. */
.equ TMD_DRAW_STREAM_G4_CORNER_NORMALS_NO_PREVIOUS_VERTEX, 0x0FFF0000

/*
 * Light all four corners and prepend the completed G4 packet to its OT bucket.
 * Inputs: t8 packet, t7 displaced OT base, a1 depth shift, t4 normal 3,
 * GTE V0..2 normals 0..2, RGBC material word and OTZ from AVSZ4.
 * Clobbers t0..t2, GTE V0, IR/MAC/FLAG and RGB FIFO; writes four
 * RGB/command words, the DMA tag with eight payload words and OT head.
 * Preserves v0 (vertex 0 XY), v1
 * (vertex 0 byte offset) and the SZ FIFO for the next element's reuse.
 * Fixed registers belong to this handler; expansion emits no call.
 * The colour stores must precede NCCS, which shifts NCCT's RGB FIFO.
 * Link arithmetic overlaps NCCS; keep the instruction schedule intact.
 */
.macro TMD_DRAW_STREAM_G4_CORNER_NORMALS_LIGHT_AND_LINK_PACKET
    sll         $t1, $t8, (32 - TMD_DRAW_STREAM_G4_CORNER_NORMALS_DMA_ADDRESS_BITS)
    srl         $t1, $t1, (32 - TMD_DRAW_STREAM_G4_CORNER_NORMALS_DMA_ADDRESS_BITS)
    ncct
    lwc2        $0, 0x0($t4)
    lwc2        $1, 0x4($t4)
    mfc2        $t2, $7
    swc2        $20, 0x4($t8)
    swc2        $21, 0xC($t8)
    swc2        $22, 0x14($t8)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_G4_CORNER_NORMALS_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_G4_CORNER_NORMALS_DEPTH_SHIFT
    sll         $t2, $t2, 2
    nccs
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_G4_CORNER_NORMALS_DMA_ADDRESS_BITS)
    lui         $t1, (TMD_DRAW_STREAM_G4_CORNER_NORMALS_PACKET_WORDS << 8)
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_G4_CORNER_NORMALS_DMA_ADDRESS_BITS)
    or          $t0, $t1, $t0
    sw          $t0, 0x0($t8)
    swc2        $22, 0x1C($t8)
.endm

glabel tmdDrawStreamPrimG4CornerNormals
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* 13F4 80010BF4 */  lw          $t9, 0x18($a0)
    /* 13F8 80010BF8 */  lw          $a3, 0x1C($a0)
    /* 13FC 80010BFC */  lw          $t8, 0x0($a0)
    /* 1400 80010C00 */  lw          $t7, 0x14($a0)
    /* 1404 80010C04 */  lw          $t6, 0x8($a0)
    /* 1408 80010C08 */  lw          $t5, 0xC($a0)
    /* 140C 80010C0C */  sll         $t9, $t9, 2
    /* 1410 80010C10 */  lui         $v1, (TMD_DRAW_STREAM_G4_CORNER_NORMALS_NO_PREVIOUS_VERTEX >> 16)
    /* 1414 80010C14 */  j           .LtmdG4CornerNormalsLoop
    /* 1418 80010C18 */  lw          $a1, 0x84($a0)
  .LtmdG4CornerNormalsProjectionRejected:
    /* A failed projection must not seed the next element's reuse. */
    /* 141C 80010C1C */  lui         $v1, (TMD_DRAW_STREAM_G4_CORNER_NORMALS_NO_PREVIOUS_VERTEX >> 16)
  .LtmdG4CornerNormalsAdvance:
    /* Every element consumes a packet slot, even when rejected. */
    /* 1420 80010C20 */  addiu       $t8, $t8, TMD_DRAW_STREAM_G4_CORNER_NORMALS_PACKET_BYTES
    /* 1424 80010C24 */  addu        $a2, $t9, $a2
  .LtmdG4CornerNormalsLoop:
    /* 1428 80010C28 */  beq         $zero, $a3, .LtmdG4CornerNormalsDone
    /* 142C 80010C2C */  nop
    /* 1430 80010C30 */  addiu       $a3, $a3, -0x1
    /* Project vertex 3 first, unless the preceding vertex 0 supplies XY and SZ. */
    /* 1434 80010C34 */  lui         $at, 0x0
    /* 1438 80010C38 */  addu        $at, $at, $a2
    /* 143C 80010C3C */  lw          $t3, 0x4($at)
    /* 1440 80010C40 */  lui         $at, 0x0
    /* 1444 80010C44 */  addu        $at, $at, $a2
    /* 1448 80010C48 */  lw          $t1, 0x0($at)
    /* 144C 80010C4C */  srl         $t4, $t3, 16
    /* 1450 80010C50 */  beq         $t4, $v1, .LtmdG4CornerNormalsReuseVertex3
    /* 1454 80010C54 */  addu        $t4, $t6, $t4
    /* 1458 80010C58 */  lwc2        $0, 0x0($t4)
    /* 145C 80010C5C */  lwc2        $1, 0x4($t4)
    /* 1460 80010C60 */  sll         $t3, $t3, 16
    /* 1464 80010C64 */  srl         $t3, $t3, 16
    /* 1468 80010C68 */  rtps
    /* 146C 80010C6C */  addu        $t3, $t6, $t3
    /* 1470 80010C70 */  srl         $t2, $t1, 16
    /* 1474 80010C74 */  addu        $t2, $t6, $t2
    /* 1478 80010C78 */  sll         $t1, $t1, 16
    /* 147C 80010C7C */  srl         $t1, $t1, 16
    /* 1480 80010C80 */  addu        $v1, $t1, $zero
    /* 1484 80010C84 */  addu        $t1, $t6, $t1
    /* 1488 80010C88 */  cfc2        $t0, $31
    /* 148C 80010C8C */  nop
    /* 1490 80010C90 */  bltz        $t0, .LtmdG4CornerNormalsProjectionRejected
    /* 1494 80010C94 */  nop
    /* 1498 80010C98 */  swc2        $14, 0x20($t8)
    /* 149C 80010C9C */  j           .LtmdG4CornerNormalsProjectOtherCorners
    /* 14A0 80010CA0 */  nop
  .LtmdG4CornerNormalsReuseVertex3:
    /* 14A4 80010CA4 */  sll         $t3, $t3, 16
    /* 14A8 80010CA8 */  srl         $t3, $t3, 16
    /* 14AC 80010CAC */  addu        $t3, $t6, $t3
    /* 14B0 80010CB0 */  srl         $t2, $t1, 16
    /* 14B4 80010CB4 */  addu        $t2, $t6, $t2
    /* 14B8 80010CB8 */  sll         $t1, $t1, 16
    /* 14BC 80010CBC */  srl         $t1, $t1, 16
    /* 14C0 80010CC0 */  addu        $v1, $t1, $zero
    /* 14C4 80010CC4 */  addu        $t1, $t6, $t1
    /* 14C8 80010CC8 */  sw          $v0, 0x20($t8)
  .LtmdG4CornerNormalsProjectOtherCorners:
    /* RTPT(2,1,0) shifts vertex 3's depth into SZ0 for the later AVSZ4. */
    /* 14CC 80010CCC */  lwc2        $0, 0x0($t3)
    /* 14D0 80010CD0 */  lwc2        $1, 0x4($t3)
    /* 14D4 80010CD4 */  lwc2        $2, 0x0($t2)
    /* 14D8 80010CD8 */  lwc2        $3, 0x4($t2)
    /* 14DC 80010CDC */  lwc2        $4, 0x0($t1)
    /* 14E0 80010CE0 */  lwc2        $5, 0x4($t1)
    /* 14E4 80010CE4 */  nop
    /* 14E8 80010CE8 */  nop
    /* 14EC 80010CEC */  rtpt
    /* Decode the four normal byte offsets while projection completes. */
    /* 14F0 80010CF0 */  lui         $at, 0x0
    /* 14F4 80010CF4 */  addu        $at, $at, $a2
    /* 14F8 80010CF8 */  lw          $t1, 0x8($at)
    /* 14FC 80010CFC */  lui         $at, 0x0
    /* 1500 80010D00 */  addu        $at, $at, $a2
    /* 1504 80010D04 */  lw          $t3, 0xC($at)
    /* 1508 80010D08 */  srl         $t2, $t1, 16
    /* 150C 80010D0C */  addu        $t2, $t5, $t2
    /* 1510 80010D10 */  sll         $t1, $t1, 16
    /* 1514 80010D14 */  srl         $t1, $t1, 16
    /* 1518 80010D18 */  addu        $t1, $t5, $t1
    /* 151C 80010D1C */  cfc2        $t0, $31
    /* 1520 80010D20 */  srl         $t4, $t3, 16
    /* 1524 80010D24 */  bltz        $t0, .LtmdG4CornerNormalsProjectionRejected
    /* 1528 80010D28 */  nop
    /* 152C 80010D2C */  addu        $t4, $t5, $t4
    /* 1530 80010D30 */  sll         $t3, $t3, 16
    /* Keep either forward-facing triangle and cache vertex 0 XY for reuse. */
    /* 1534 80010D34 */  nclip
    /* 1538 80010D38 */  mfc2        $v0, $14
    /* 153C 80010D3C */  swc2        $12, 0x18($t8)
    /* 1540 80010D40 */  swc2        $13, 0x10($t8)
    /* 1544 80010D44 */  sw          $v0, 0x8($t8)
    /* 1548 80010D48 */  mfc2        $t0, $24
    /* 154C 80010D4C */  nop
    /* 1550 80010D50 */  bltz        $t0, .LtmdG4CornerNormalsLightAndLink
    /* 1554 80010D54 */  nop
    /* 1558 80010D58 */  lwc2        $14, 0x20($t8)
    /* 155C 80010D5C */  nop
    /* 1560 80010D60 */  nop
    /* 1564 80010D64 */  nclip
    /* 1568 80010D68 */  mfc2        $t0, $24
    /* 156C 80010D6C */  nop
    /* 1570 80010D70 */  blez        $t0, .LtmdG4CornerNormalsAdvance
    /* 1574 80010D74 */  nop
  .LtmdG4CornerNormalsLightAndLink:
    /* 1578 80010D78 */  srl         $t3, $t3, 16
    /* 157C 80010D7C */  addu        $t3, $t5, $t3
    /* Average all four depths and load the material colour and first three normals. */
    /* 1580 80010D80 */  avsz4
    /* 1584 80010D84 */  lui         $at, 0x0
    /* 1588 80010D88 */  addu        $at, $at, $a2
    /* 158C 80010D8C */  lwc2        $6, 0x10($at)
    /* 1590 80010D90 */  lwc2        $0, 0x0($t1)
    /* 1594 80010D94 */  lwc2        $1, 0x4($t1)
    /* 1598 80010D98 */  lwc2        $2, 0x0($t2)
    /* 159C 80010D9C */  lwc2        $3, 0x4($t2)
    /* 15A0 80010DA0 */  lwc2        $4, 0x0($t3)
    /* 15A4 80010DA4 */  lwc2        $5, 0x4($t3)
    /* 15A8..1600 80010DA8..80010E00 */  TMD_DRAW_STREAM_G4_CORNER_NORMALS_LIGHT_AND_LINK_PACKET
    /* 1604 80010E04 */  j           .LtmdG4CornerNormalsAdvance
    /* 1608 80010E08 */  nop
  .LtmdG4CornerNormalsDone:
    /* 160C 80010E0C */  sw          $t8, 0x0($a0)
    /* 1610 80010E10 */  addu        $v0, $zero, $a2
    /* 1614 80010E14 */  jr          $ra
    /* 1618 80010E18 */  nop
endlabel tmdDrawStreamPrimG4CornerNormals
.purgem TMD_DRAW_STREAM_G4_CORNER_NORMALS_LIGHT_AND_LINK_PACKET
/* Outside the unsigned 16-bit vertex byte-offset domain. */
.equ TMD_XFORM_STREAM_VERTS_ELEM_COLOR_NO_PREVIOUS_VERTEX, 0x0FFE0000
/* Assembly equivalent of the depth-cache rejection bit in main/tmd_types.h. */
.equ TMD_XFORM_STREAM_VERTS_ELEM_COLOR_DEPTH_INVALID, 0x80000000

/*
 * Light one normal and scatter the projected XY and lit colour.
 * Inputs: a2 element, t4 its destination word (element word 2), t9 byte stride,
 * t8 packet base; GTE V0 holds the normal, SXY2 the projected vertex, and RGB
 * the element's material colour. Advances a2; clobbers t3, t4 and the NCCS
 * results. Both halves of t4 are unsigned 16-bit byte offsets from t8 and must
 * name complete aligned four-byte words. The colour half is decoded into t3
 * and the screen-XY half reuses t4. The fixed-colour pass loads its destination
 * word inside its own expansion and assigns those halves to the opposite
 * registers, so the two schedules stay separate. Offset decoding supplies the
 * NCCS result latency; keep the stores in order even when the destinations
 * coincide. Expansion emits no call or extra delays.
 */
.macro TMD_XFORM_STREAM_VERTS_ELEM_COLOR_SCATTER
    addu        $a2, $t9, $a2
    nccs
    srl         $t3, $t4, 16
    addu        $t3, $t8, $t3
    sll         $t4, $t4, 16
    srl         $t4, $t4, 16
    addu        $t4, $t8, $t4
    swc2        $14, 0x0($t4)
    swc2        $22, 0x0($t3)
.endm

glabel tmdXformStreamVertsElemColor
    /* a0 workspace, a1 unused object flags, a2 elements; a3 counts locally. */
    /* t8 packet base, t6 vertices, t5 normals, v1 depth-cache base. */
    /* 161C 80010E1C */  lw          $t9, 0x18($a0)
    /* 1620 80010E20 */  lw          $a3, 0x1C($a0)
    /* 1624 80010E24 */  lw          $t8, 0x4($a0)
    /* 1628 80010E28 */  lw          $t6, 0x8($a0)
    /* 162C 80010E2C */  lw          $t5, 0xC($a0)
    /* 1630 80010E30 */  lw          $v1, 0x10($a0)
    /* 1634 80010E34 */  sll         $t9, $t9, 2
    /* 1638 80010E38 */  lui         $v0, (TMD_XFORM_STREAM_VERTS_ELEM_COLOR_NO_PREVIOUS_VERTEX >> 16)
    /* 163C 80010E3C */  j           .LtmdXformStreamVertsElemColorNextElement
    /* 1640 80010E40 */  nop
  .LtmdXformStreamVertsElemColorReuseProjection:
    /* SXY2 survives the preceding projection; this element still has its own normal and colour. */
    /* 1644 80010E44 */  addu        $t2, $t5, $t2
    /* 1648 80010E48 */  lwc2        $6, 0x4($a2)
    /* 164C 80010E4C */  lwc2        $0, 0x0($t2)
    /* 1650 80010E50 */  lwc2        $1, 0x4($t2)
    /* 1654 80010E54 */  lw          $t4, 0x8($a2)
  .LtmdXformStreamVertsElemColorScatter:
    /* 1658..1678 80010E58..80010E78 */  TMD_XFORM_STREAM_VERTS_ELEM_COLOR_SCATTER
  .LtmdXformStreamVertsElemColorNextElement:
    /* The delay-slot load also reads the word after the payload, including an empty record. */
    /* 167C 80010E7C */  beq         $zero, $a3, .LtmdXformStreamVertsElemColorReturn
    /* 1680 80010E80 */  lw          $t1, 0x0($a2)
    /* 1684 80010E84 */  addiu       $a3, $a3, -0x1
    /* 1688 80010E88 */  srl         $t2, $t1, 16
    /* 168C 80010E8C */  sll         $t1, $t1, 16
    /* 1690 80010E90 */  srl         $t1, $t1, 16
    /* 1694 80010E94 */  beq         $t1, $v0, .LtmdXformStreamVertsElemColorReuseProjection
    /* 1698 80010E98 */  addu        $v0, $zero, $t1
    /* 169C 80010E9C */  addu        $t1, $t6, $v0
    /* 16A0 80010EA0 */  lwc2        $0, 0x0($t1)
    /* 16A4 80010EA4 */  lwc2        $1, 0x4($t1)
    /* Halve the vertex byte offset to address its four-byte depth-cache entry. */
    /* 16A8 80010EA8 */  srl         $t3, $v0, 1
    /* 16AC 80010EAC */  addu        $t3, $v1, $t3
    /* Project, then load this element's normal and material colour while RTPS completes. */
    /* 16B0 80010EB0 */  rtps
    /* 16B4 80010EB4 */  addu        $t2, $t5, $t2
    /* 16B8 80010EB8 */  lwc2        $6, 0x4($a2)
    /* 16BC 80010EBC */  lwc2        $0, 0x0($t2)
    /* 16C0 80010EC0 */  lwc2        $1, 0x4($t2)
    /* Cache SZ3 even on projection failure; later primitives reject a negative depth. */
    /* 16C4 80010EC4 */  cfc2        $t1, $31
    /* 16C8 80010EC8 */  mfc2        $t0, $19
    /* 16CC 80010ECC */  bgez        $t1, .LtmdXformStreamVertsElemColorStoreDepth
    /* 16D0 80010ED0 */  lui         $t4, (TMD_XFORM_STREAM_VERTS_ELEM_COLOR_DEPTH_INVALID >> 16)
    /* 16D4 80010ED4 */  or          $t0, $t4, $t0
  .LtmdXformStreamVertsElemColorStoreDepth:
    /* Word 2 is loaded before the store: t3 still addresses the cache entry. */
    /* 16D8 80010ED8 */  lw          $t4, 0x8($a2)
    /* 16DC 80010EDC */  sw          $t0, 0x0($t3)
    /* 16E0 80010EE0 */  j           .LtmdXformStreamVertsElemColorScatter
    /* 16E4 80010EE4 */  nop
  .LtmdXformStreamVertsElemColorReturn:
    /* 16E8 80010EE8 */  addu        $v0, $zero, $a2
    /* 16EC 80010EEC */  jr          $ra
    /* 16F0 80010EF0 */  nop
.purgem TMD_XFORM_STREAM_VERTS_ELEM_COLOR_SCATTER
/* GPU command 0x36 and neutral RGB for per-corner texture lighting. */
.equ TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR, 0x36808080

alabel tmdDrawStreamGt3SemiTrans
    /* Force the blended command; the shared walk still selects facing from a1. */
    /* 16F4 80010EF4 */  lui         $t0, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR >> 16)
    /* 16F8 80010EF8 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR & 0xFFFF)
    /* 16FC 80010EFC */  mtc2        $t0, $6
    /* 1700 80010F00 */  j           .L80010F20
    /* 1704 80010F04 */  nop

/* Assembly equivalents of the object-flag masks in main/tmd_types.h. */
.equ TMD_OBJECT_SEMI_TRANS, 0x02
.equ TMD_OBJECT_REVERSE_CULLING, 0x10
/* POLY_GT3 storage includes its DMA tag; the length excludes that tag. */
.equ TMD_DRAW_STREAM_GT3_PACKET_BYTES, 40
.equ TMD_DRAW_STREAM_GT3_PACKET_WORDS, (TMD_DRAW_STREAM_GT3_PACKET_BYTES / 4) - 1
/* GPU command 0x34 and neutral RGB for per-corner texture lighting. */
.equ TMD_DRAW_STREAM_GT3_COLOR, 0x34808080
/* Keep 14 scaled depth bits, then quantize by 16 into 1024 OT entries. */
.equ TMD_DRAW_STREAM_GT3_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_GT3_DEPTH_SHIFT, 4

/*
 * Complete and prepend one GT3 packet to its wrapped depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 OTZ, a1 depth shift, v0 nine-word DMA length in bits 24..31,
 * GTE RGB0..2 lit corner colours. Clobbers t0 and t2; writes the OT head,
 * packet tag and packet colour words.
 * Both facing walks use this expansion; it emits no call or extra delays.
 */
.macro TMD_DRAW_STREAM_GT3_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, 8
    srl         $t0, $t0, 8
    or          $t0, $v0, $t0
    sw          $t0, 0x0($t8)
    swc2        $20, 0x4($t8)
    swc2        $21, 0x10($t8)
    swc2        $22, 0x1C($t8)
.endm

glabel tmdDrawStreamGt3
    /* a0 workspace, a1 object flags, a2 first element word. */
    /* 1708 80010F08 */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 170C 80010F0C */  bnez        $t0, tmdDrawStreamGt3SemiTrans
    /* 1710 80010F10 */  nop
    /* 1714 80010F14 */  lui         $t0, (TMD_DRAW_STREAM_GT3_COLOR >> 16)
    /* 1718 80010F18 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_COLOR & 0xFFFF)
    /* 171C 80010F1C */  mtc2        $t0, $6
  .L80010F20:
    /* 1720 80010F20 */  lw          $t9, 0x18($a0)
    /* 1724 80010F24 */  lw          $a3, 0x1C($a0)
    /* 1728 80010F28 */  lw          $t8, 0x0($a0)
    /* 172C 80010F2C */  lw          $t7, 0x14($a0)
    /* 1730 80010F30 */  lw          $t6, 0x8($a0)
    /* 1734 80010F34 */  lw          $t5, 0xC($a0)
    /* 1738 80010F38 */  sll         $t9, $t9, 2
    /* 173C 80010F3C */  lui         $v0, (TMD_DRAW_STREAM_GT3_PACKET_WORDS << 8)
    /* 1740 80010F40 */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 1744 80010F44 */  bnez        $t1, .L80012064
    /* 1748 80010F48 */  lw          $a1, 0x84($a0)
    /* 174C 80010F4C */  j           .L80010F5C
    /* 1750 80010F50 */  nop
  .L80010F54:
    /* Rejection still consumes the packet slot and the element's word stride. */
    /* 1754 80010F54 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
  .L80010F58:
    /* 1758 80010F58 */  addu        $a2, $t9, $a2
  .L80010F5C:
    /* 175C 80010F5C */  beq         $zero, $a3, .L8001107C
    /* 1760 80010F60 */  nop
    /* 1764 80010F64 */  addiu       $a3, $a3, -0x1
    /* Unpack three vertex and three normal byte offsets, two per word. */
    /* 1768 80010F68 */  lw          $t1, 0x0($a2)
    /* 176C 80010F6C */  lw          $t3, 0x4($a2)
    /* 1770 80010F70 */  srl         $t2, $t1, 16
    /* 1774 80010F74 */  addu        $t2, $t6, $t2
    /* 1778 80010F78 */  sll         $t1, $t1, 16
    /* 177C 80010F7C */  srl         $t1, $t1, 16
    /* 1780 80010F80 */  addu        $t1, $t6, $t1
    /* 1784 80010F84 */  srl         $t4, $t3, 16
    /* 1788 80010F88 */  sll         $t3, $t3, 16
    /* 178C 80010F8C */  srl         $t3, $t3, 16
    /* 1790 80010F90 */  addu        $t3, $t6, $t3
    /* 1794 80010F94 */  lwc2        $0, 0x0($t1)
    /* 1798 80010F98 */  lwc2        $1, 0x4($t1)
    /* 179C 80010F9C */  lwc2        $2, 0x0($t2)
    /* 17A0 80010FA0 */  lwc2        $3, 0x4($t2)
    /* 17A4 80010FA4 */  lwc2        $4, 0x0($t3)
    /* 17A8 80010FA8 */  lwc2        $5, 0x4($t3)
    /* 17AC 80010FAC */  lw          $t2, 0x8($a2)
    /* 17B0 80010FB0 */  sll         $t1, $t4, 0
    /* 17B4 80010FB4 */  rtpt
    /* 17B8 80010FB8 */  addu        $t1, $t5, $t1
    /* 17BC 80010FBC */  srl         $t3, $t2, 16
    /* 17C0 80010FC0 */  addu        $t3, $t5, $t3
    /* 17C4 80010FC4 */  sll         $t2, $t2, 16
    /* 17C8 80010FC8 */  srl         $t2, $t2, 16
    /* 17CC 80010FCC */  addu        $t2, $t5, $t2
    /* 17D0 80010FD0 */  cfc2        $t0, $31
    /* 17D4 80010FD4 */  nop
    /* 17D8 80010FD8 */  bltz        $t0, .L80010F54
    /* 17DC 80010FDC */  nop
    /* 17E0 80010FE0 */  nop
    /* 17E4 80010FE4 */  nop
    /* Project first; normals affect lighting, while screen winding decides facing. */
    /* 17E8 80010FE8 */  nclip
    /* 17EC 80010FEC */  mfc2        $t0, $24
    /* 17F0 80010FF0 */  nop
    /* 17F4 80010FF4 */  blez        $t0, .L80010F54
    /* 17F8 80010FF8 */  nop
    /* 17FC 80010FFC */  swc2        $12, 0x8($t8)
    /* 1800 80011000 */  swc2        $13, 0x14($t8)
    /* 1804 80011004 */  swc2        $14, 0x20($t8)
    /* 1808 80011008 */  nop
    /* 180C 8001100C */  nop
    /* 1810 80011010 */  avsz3
    /* 1814 80011014 */  lwc2        $0, 0x0($t1)
    /* 1818 80011018 */  lwc2        $1, 0x4($t1)
    /* 181C 8001101C */  lwc2        $2, 0x0($t2)
    /* 1820 80011020 */  lwc2        $3, 0x4($t2)
    /* 1824 80011024 */  lwc2        $4, 0x0($t3)
    /* 1828 80011028 */  lwc2        $5, 0x4($t3)
    /* 182C 8001102C */  mfc2        $t2, $7
    /* 1830 80011030 */  sll         $t1, $t8, 8
    /* 1834 80011034 */  srl         $t1, $t1, 8
    /* Light all three normals, then link using 24-bit DMA addresses. */
    /* 1838 80011038 */  ncct
    /* 183C..1870 8001103C..80011070 */  TMD_DRAW_STREAM_GT3_LINK_PACKET
    /* 1874 80011074 */  j           .L80010F54
    /* 1878 80011078 */  nop
  .L8001107C:
    /* 187C 8001107C */  sw          $t8, 0x0($a0)
  .L80011080:
    /* 1880 80011080 */  addu        $v0, $zero, $a2
    /* 1884 80011084 */  jr          $ra
    /* 1888 80011088 */  nop
/* POLY_GT4 storage includes its DMA tag; the length excludes that tag. */
.equ TMD_DRAW_STREAM_GT4_PACKET_BYTES, 52
.equ TMD_DRAW_STREAM_GT4_PACKET_WORDS, (TMD_DRAW_STREAM_GT4_PACKET_BYTES / 4) - 1
/* GPU command 0x3C and neutral RGB for per-corner texture lighting. */
.equ TMD_DRAW_STREAM_GT4_COLOR, 0x3C808080
/* Outside the u16 vertex-offset domain; replaced with each element's vertex 0. */
.equ TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX, 0x0FFF0000
/* Keep 14 scaled depth bits, then quantize by 16 into 1024 OT entries. */
.equ TMD_DRAW_STREAM_GT4_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_GT4_DEPTH_SHIFT, 4
.equ TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS, 24

/*
 * Light corner 3 and prepend one GT4 packet to its wrapped depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ4 OTZ, a1 depth shift; GTE V0 holds normal 3 and RGB the material.
 * NCCT colours for corners 0..2 must already be stored in the packet.
 * Clobbers t0, t1, t2 and the NCCS GTE results; writes the OT head, packet
 * tag and corner-3 colour. v0/v1 retain the vertex-0 screen XY/cache key.
 * Both facing walks use this expansion. Keep NCCS interleaved with DMA
 * insertion: the intervening instructions provide its result latency.
 */
.macro TMD_DRAW_STREAM_GT4_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_SHIFT
    sll         $t2, $t2, 2
    nccs
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    lui         $t1, (TMD_DRAW_STREAM_GT4_PACKET_WORDS << (TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS - 16))
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    or          $t0, $t1, $t0
    sw          $t0, 0x0($t8)
    swc2        $22, 0x28($t8)
.endm

/* GPU command 0x3E with neutral RGB for semi-transparent per-corner lighting. */
.equ TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR, 0x3E808080

alabel tmdDrawStreamGt4SemiTrans
    /* Force the blended code; a1 still selects the shared walk's facing rule. */
    /* 188C 8001108C */  lui         $t0, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR >> 16)
    /* 1890 80011090 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR & 0xFFFF)
    /* 1894 80011094 */  mtc2        $t0, $6
    /* 1898 80011098 */  j           .L800110B8
    /* 189C 8001109C */  nop
glabel tmdDrawStreamGt4
    /* a0 workspace, a1 object flags, a2 first element word. */
    /* 18A0 800110A0 */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 18A4 800110A4 */  bnez        $t0, tmdDrawStreamGt4SemiTrans
    /* 18A8 800110A8 */  nop
    /* 18AC 800110AC */  lui         $t0, (TMD_DRAW_STREAM_GT4_COLOR >> 16)
    /* 18B0 800110B0 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_COLOR & 0xFFFF)
    /* 18B4 800110B4 */  mtc2        $t0, $6
  .L800110B8:
    /* t9 stride bytes, a3 remaining elements, t8 packet, t7 OT, t6/t5 geometry. */
    /* 18B8 800110B8 */  lw          $t9, 0x18($a0)
    /* 18BC 800110BC */  lw          $a3, 0x1C($a0)
    /* 18C0 800110C0 */  lw          $t8, 0x0($a0)
    /* 18C4 800110C4 */  lw          $t7, 0x14($a0)
    /* 18C8 800110C8 */  lw          $t6, 0x8($a0)
    /* 18CC 800110CC */  lw          $t5, 0xC($a0)
    /* 18D0 800110D0 */  sll         $t9, $t9, 2
    /* 18D4 800110D4 */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
    /* 18D8 800110D8 */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 18DC 800110DC */  bnez        $t1, .L80011E98
    /* 18E0 800110E0 */  lw          $a1, 0x84($a0)
    /* 18E4 800110E4 */  j           .L800110F8
    /* 18E8 800110E8 */  nop
  .L800110EC:
    /* Failed projection invalidates the ordinary walk's vertex-reuse key. */
    /* 18EC 800110EC */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
  .L800110F0:
    /* Rejection still consumes a packet slot and the element's word stride. */
    /* 18F0 800110F0 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
  .L800110F4:
    /* 18F4 800110F4 */  addu        $a2, $t9, $a2
  .L800110F8:
    /* 18F8 800110F8 */  beq         $zero, $a3, .L800112CC
    /* 18FC 800110FC */  nop
    /* 1900 80011100 */  addiu       $a3, $a3, -0x1
    /* 1904 80011104 */  lui         $at, 0x0
    /* 1908 80011108 */  addu        $at, $at, $a2
    /* 190C 8001110C */  lw          $t3, 0x4($at)
    /* 1910 80011110 */  lui         $at, 0x0
    /* 1914 80011114 */  addu        $at, $at, $a2
    /* 1918 80011118 */  lw          $t1, 0x0($at)
    /* Reuse vertex 3 when it is the preceding element's vertex 0 (v1 key, v0 XY). */
    /* 191C 8001111C */  srl         $t4, $t3, 16
    /* 1920 80011120 */  beq         $t4, $v1, .L80011174
    /* 1924 80011124 */  addu        $t4, $t6, $t4
    /* 1928 80011128 */  lwc2        $0, 0x0($t4)
    /* 192C 8001112C */  lwc2        $1, 0x4($t4)
    /* 1930 80011130 */  sll         $t3, $t3, 16
    /* 1934 80011134 */  srl         $t3, $t3, 16
    /* 1938 80011138 */  rtps
    /* 193C 8001113C */  addu        $t3, $t6, $t3
    /* 1940 80011140 */  srl         $t2, $t1, 16
    /* 1944 80011144 */  addu        $t2, $t6, $t2
    /* 1948 80011148 */  sll         $t1, $t1, 16
    /* 194C 8001114C */  srl         $t1, $t1, 16
    /* 1950 80011150 */  addu        $v1, $t1, $zero
    /* 1954 80011154 */  addu        $t1, $t6, $t1
    /* Signed FLAG tests reject the full TMD_GTE_ERROR_FLAG projection summary. */
    /* 1958 80011158 */  cfc2        $t0, $31
    /* 195C 8001115C */  nop
    /* 1960 80011160 */  bltz        $t0, .L800110EC
    /* 1964 80011164 */  nop
    /* 1968 80011168 */  swc2        $14, 0x2C($t8)
    /* 196C 8001116C */  j           .L8001119C
    /* 1970 80011170 */  nop
  .L80011174:
    /* 1974 80011174 */  sll         $t3, $t3, 16
  .L80011178:
    /* 1978 80011178 */  srl         $t3, $t3, 16
    /* 197C 8001117C */  addu        $t3, $t6, $t3
    /* 1980 80011180 */  srl         $t2, $t1, 16
    /* 1984 80011184 */  addu        $t2, $t6, $t2
    /* 1988 80011188 */  sll         $t1, $t1, 16
    /* 198C 8001118C */  srl         $t1, $t1, 16
    /* 1990 80011190 */  addu        $v1, $t1, $zero
    /* 1994 80011194 */  addu        $t1, $t6, $t1
    /* 1998 80011198 */  sw          $v0, 0x2C($t8)
  .L8001119C:
    /* Project vertices 2, 1, 0; the depth FIFO retains vertex 3 for AVSZ4. */
    /* 199C 8001119C */  lwc2        $0, 0x0($t3)
    /* 19A0 800111A0 */  lwc2        $1, 0x4($t3)
    /* 19A4 800111A4 */  lwc2        $2, 0x0($t2)
    /* 19A8 800111A8 */  lwc2        $3, 0x4($t2)
    /* 19AC 800111AC */  lwc2        $4, 0x0($t1)
    /* 19B0 800111B0 */  lwc2        $5, 0x4($t1)
    /* 19B4 800111B4 */  nop
    /* 19B8 800111B8 */  nop
    /* 19BC 800111BC */  rtpt
    /* 19C0 800111C0 */  lui         $at, 0x0
    /* 19C4 800111C4 */  addu        $at, $at, $a2
    /* 19C8 800111C8 */  lw          $t1, 0x8($at)
    /* 19CC 800111CC */  lui         $at, 0x0
    /* 19D0 800111D0 */  addu        $at, $at, $a2
    /* 19D4 800111D4 */  lw          $t3, 0xC($at)
    /* 19D8 800111D8 */  srl         $t2, $t1, 16
    /* 19DC 800111DC */  addu        $t2, $t5, $t2
    /* 19E0 800111E0 */  sll         $t1, $t1, 16
    /* 19E4 800111E4 */  srl         $t1, $t1, 16
    /* 19E8 800111E8 */  addu        $t1, $t5, $t1
    /* 19EC 800111EC */  cfc2        $t0, $31
    /* 19F0 800111F0 */  srl         $t4, $t3, 16
    /* 19F4 800111F4 */  bltz        $t0, .L800110EC
    /* 19F8 800111F8 */  nop
    /* 19FC 800111FC */  addu        $t4, $t5, $t4
    /* 1A00 80011200 */  sll         $t3, $t3, 16
    /* Test triangle (2,1,0), then (2,1,3) only if the first is not kept. */
    /* 1A04 80011204 */  nclip
    /* 1A08 80011208 */  mfc2        $v0, $14
    /* 1A0C 8001120C */  swc2        $12, 0x20($t8)
    /* 1A10 80011210 */  swc2        $13, 0x14($t8)
    /* 1A14 80011214 */  sw          $v0, 0x8($t8)
    /* 1A18 80011218 */  mfc2        $t0, $24
    /* 1A1C 8001121C */  nop
    /* 1A20 80011220 */  bltz        $t0, .L80011244
    /* 1A24 80011224 */  lwc2        $14, 0x2C($t8)
    /* 1A28 80011228 */  nop
    /* 1A2C 8001122C */  nop
    /* 1A30 80011230 */  nclip
    /* 1A34 80011234 */  mfc2        $t0, $24
    /* 1A38 80011238 */  nop
    /* 1A3C 8001123C */  blez        $t0, .L800110F0
    /* 1A40 80011240 */  nop
  .L80011244:
    /* 1A44 80011244 */  srl         $t3, $t3, 16
  .L80011248:
    /* 1A48 80011248 */  addu        $t3, $t5, $t3
    /* Average four depths and light normals 0..2 together, then normal 3. */
    /* 1A4C 8001124C */  avsz4
    /* 1A50 80011250 */  lwc2        $0, 0x0($t1)
    /* 1A54 80011254 */  lwc2        $1, 0x4($t1)
    /* 1A58 80011258 */  lwc2        $2, 0x0($t2)
    /* 1A5C 8001125C */  lwc2        $3, 0x4($t2)
    /* 1A60 80011260 */  lwc2        $4, 0x0($t3)
    /* 1A64 80011264 */  lwc2        $5, 0x4($t3)
    /* 1A68 80011268 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 1A6C 8001126C */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 1A70 80011270 */  ncct
    /* 1A74 80011274 */  lwc2        $0, 0x0($t4)
    /* 1A78 80011278 */  lwc2        $1, 0x4($t4)
    /* 1A7C 8001127C */  mfc2        $t2, $7
    /* 1A80 80011280 */  swc2        $20, 0x4($t8)
    /* 1A84 80011284 */  swc2        $21, 0x10($t8)
    /* 1A88 80011288 */  swc2        $22, 0x1C($t8)
    /* 1A8C..1AC0 8001128C..800112C0 */  TMD_DRAW_STREAM_GT4_LINK_PACKET
    /* 1AC4 800112C4 */  j           .L800110F0
    /* 1AC8 800112C8 */  nop
  .L800112CC:
    /* 1ACC 800112CC */  sw          $t8, 0x0($a0)
  .L800112D0:
    /* 1AD0 800112D0 */  addu        $v0, $zero, $a2
    /* 1AD4 800112D4 */  jr          $ra
    /* 1AD8 800112D8 */  nop
/* Neutral RGB with a zero code byte; RGB2 stores all four bytes. */
.equ TMD_XFORM_STREAM_VERTS_COLOR, 0x00808080
/* Outside the unsigned 16-bit vertex byte-offset domain. */
.equ TMD_XFORM_STREAM_VERTS_NO_PREVIOUS_VERTEX, 0x0FFE0000
/* Assembly equivalent of the depth-cache rejection bit in main/tmd_types.h. */
.equ TMD_XFORM_STREAM_VERTS_DEPTH_INVALID, 0x80000000

/*
 * Light one normal and scatter the projected XY and lit colour to packet words.
 * Inputs: a2 element (refs, destinations), t9 byte stride, t8 packet base;
 * GTE V0 holds the normal, SXY2 the projected vertex, RGB the neutral colour.
 * Advances a2; clobbers t3, t4 and NCCS results. Both destinations are unsigned
 * 16-bit byte offsets from t8 and must name complete aligned four-byte words.
 * Offset decoding supplies the NCCS result latency; keep the stores in order
 * even when the two destinations coincide. No call or extra delays are emitted.
 */
.macro TMD_XFORM_STREAM_VERTS_LIGHT_AND_SCATTER
    lw          $t3, 0x4($a2)
    addu        $a2, $t9, $a2
    nccs
    srl         $t4, $t3, 16
    addu        $t4, $t8, $t4
    sll         $t3, $t3, 16
    srl         $t3, $t3, 16
    addu        $t3, $t8, $t3
    swc2        $14, 0x0($t3)
    swc2        $22, 0x0($t4)
.endm

glabel tmdXformStreamVerts
    /* a0 workspace, a1 unused object flags, a2 elements; a3 counts locally. */
    /* t8 fixed packet base, t6 vertices, t5 normals, v1 depth-cache base. */
    /* 1ADC 800112DC */  lw          $t9, 0x18($a0)
    /* 1AE0 800112E0 */  lw          $a3, 0x1C($a0)
    /* 1AE4 800112E4 */  lw          $t8, 0x4($a0)
    /* 1AE8 800112E8 */  lw          $t6, 0x8($a0)
    /* 1AEC 800112EC */  lw          $t5, 0xC($a0)
    /* 1AF0 800112F0 */  lw          $v1, 0x10($a0)
    /* 1AF4 800112F4 */  sll         $t9, $t9, 2
    /* 1AF8 800112F8 */  lui         $t1, (TMD_XFORM_STREAM_VERTS_COLOR >> 16)
    /* 1AFC 800112FC */  ori         $t1, $t1, (TMD_XFORM_STREAM_VERTS_COLOR & 0xFFFF)
    /* 1B00 80011300 */  mtc2        $t1, $6
    /* 1B04 80011304 */  lui         $v0, (TMD_XFORM_STREAM_VERTS_NO_PREVIOUS_VERTEX >> 16)
    /* 1B08 80011308 */  j           .LtmdXformStreamVertsNextElement
    /* 1B0C 8001130C */  nop
  .LtmdXformStreamVertsReuseProjection:
    /* SXY2 and cached Z survive lighting; a repeated vertex still has its own normal. */
    /* 1B10 80011310 */  addu        $t2, $t5, $t2
    /* 1B14 80011314 */  lwc2        $0, 0x0($t2)
    /* 1B18 80011318 */  lwc2        $1, 0x4($t2)
  .LtmdXformStreamVertsLightAndScatter:
    /* 1B1C..1B40 8001131C..80011340 */  TMD_XFORM_STREAM_VERTS_LIGHT_AND_SCATTER
  .LtmdXformStreamVertsNextElement:
    /* The delay-slot load also reads the word after the payload, including an empty record. */
    /* 1B44 80011344 */  beq         $zero, $a3, .LtmdXformStreamVertsReturn
    /* 1B48 80011348 */  lw          $t1, 0x0($a2)
    /* 1B4C 8001134C */  addiu       $a3, $a3, -0x1
    /* 1B50 80011350 */  srl         $t2, $t1, 16
    /* 1B54 80011354 */  sll         $t1, $t1, 16
    /* 1B58 80011358 */  srl         $t1, $t1, 16
    /* 1B5C 8001135C */  beq         $t1, $v0, .LtmdXformStreamVertsReuseProjection
    /* 1B60 80011360 */  addu        $v0, $zero, $t1
    /* 1B64 80011364 */  addu        $t1, $t6, $v0
    /* 1B68 80011368 */  lwc2        $0, 0x0($t1)
    /* 1B6C 8001136C */  lwc2        $1, 0x4($t1)
    /* Halve the vertex byte offset to address its four-byte depth-cache entry. */
    /* 1B70 80011370 */  srl         $t3, $v0, 1
    /* 1B74 80011374 */  addu        $t3, $v1, $t3
    /* 1B78 80011378 */  rtps
    /* 1B7C 8001137C */  addu        $t2, $t5, $t2
    /* 1B80 80011380 */  lwc2        $0, 0x0($t2)
    /* 1B84 80011384 */  lwc2        $1, 0x4($t2)
    /* Cache SZ3 even on projection failure; later primitives reject a negative depth. */
    /* 1B88 80011388 */  cfc2        $t1, $31
    /* 1B8C 8001138C */  mfc2        $t0, $19
    /* 1B90 80011390 */  bgez        $t1, .LtmdXformStreamVertsStoreDepth
    /* 1B94 80011394 */  lui         $t4, (TMD_XFORM_STREAM_VERTS_DEPTH_INVALID >> 16)
    /* 1B98 80011398 */  or          $t0, $t4, $t0
  .LtmdXformStreamVertsStoreDepth:
    /* 1B9C 8001139C */  sw          $t0, 0x0($t3)
    /* 1BA0 800113A0 */  j           .LtmdXformStreamVertsLightAndScatter
    /* 1BA4 800113A4 */  nop
  .LtmdXformStreamVertsReturn:
    /* 1BA8 800113A8 */  addu        $v0, $zero, $a2
    /* 1BAC 800113AC */  jr          $ra
    /* 1BB0 800113B0 */  nop
.purgem TMD_XFORM_STREAM_VERTS_LIGHT_AND_SCATTER
/* Take the semi-transparent GT3 command byte; pre-pass RGB remains in the packet. */
.equ TMD_DRAW_STREAM_GT3_PRE_XFORM_SEMI_TRANS_CODE, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR >> 24)

alabel tmdDrawStreamPrimGt3PreXformSemiTrans
    /* a0 workspace, a1 object flags, a2 elements; force blending, retain facing selection. */
    /* t8 first-region packet cursor, t6 depth cache, t7 displaced OT base. */
    /* 1BB4 800113B4 */  lw          $t9, 0x18($a0)
    /* 1BB8 800113B8 */  lw          $a3, 0x1C($a0)
    /* 1BBC 800113BC */  lw          $t8, 0x4($a0)
    /* 1BC0 800113C0 */  lw          $t7, 0x14($a0)
    /* 1BC4 800113C4 */  lw          $t6, 0x10($a0)
    /* 1BC8 800113C8 */  sll         $t9, $t9, 2
    /* 1BCC 800113CC */  lui         $v0, (TMD_DRAW_STREAM_GT3_PACKET_WORDS << 8)
    /* 1BD0 800113D0 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT3_PRE_XFORM_SEMI_TRANS_CODE
    /* 1BD4 800113D4 */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 1BD8 800113D8 */  bnez        $t1, .LtmdGt3PreXformReverseLoop
    /* 1BDC 800113DC */  lw          $a1, 0x84($a0)
    /* 1BE0 800113E0 */  j           .LtmdGt3PreXformLoop
    /* 1BE4 800113E4 */  nop

/* Reuse the GT3 packet/depth constants; only the command byte is needed here. */
.equ TMD_DRAW_STREAM_GT3_PRE_XFORM_OPAQUE_CODE, (TMD_DRAW_STREAM_GT3_COLOR >> 24)
/* GPU linked-list addresses retain the low 24 bits of a packet address. */
.equ TMD_DRAW_STREAM_GT3_PRE_XFORM_DMA_ADDRESS_MASK, 0x00FFFFFF

/*
 * Complete the packet's command/tag and prepend it to its wrapped depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ3 OTZ, a1 depth shift, t0 DMA address mask, v1 GPU command byte,
 * v0 nine-word DMA length in bits 24..31. Clobbers t2 and t5; writes the
 * OT head, packet tag and command byte, retaining all XY, RGB and texture data.
 * Both facing walks and the semi-transparent entry use this expansion.
 * Fixed registers preserve the load/GTE delays; it emits no call or extra
 * instructions and is purged after this body's reverse-culling walk.
 */
.macro TMD_DRAW_STREAM_GT3_PRE_XFORM_LINK_PACKET
    sb          $v1, 0x7($t8)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t5, 0x0($t2)
    sw          $t1, 0x0($t2)
    and         $t5, $t0, $t5
    or          $t5, $v0, $t5
    sw          $t5, 0x0($t8)
.endm

glabel tmdDrawStreamPrimGt3PreXform
    /* a0 workspace, a1 object flags, a2 first element word; a3 counts locally. */
    /* t8 first-region packet cursor, t6 depth cache, t7 displaced OT base. */
    /* 1BE8 800113E8 */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 1BEC 800113EC */  bnez        $t0, tmdDrawStreamPrimGt3PreXformSemiTrans
    /* 1BF0 800113F0 */  nop
    /* 1BF4 800113F4 */  lw          $t9, 0x18($a0)
    /* 1BF8 800113F8 */  lw          $a3, 0x1C($a0)
    /* 1BFC 800113FC */  lw          $t8, 0x4($a0)
    /* 1C00 80011400 */  lw          $t7, 0x14($a0)
    /* 1C04 80011404 */  lw          $t6, 0x10($a0)
    /* 1C08 80011408 */  sll         $t9, $t9, 2
    /* 1C0C 8001140C */  lui         $v0, (TMD_DRAW_STREAM_GT3_PACKET_WORDS << 8)
    /* 1C10 80011410 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT3_PRE_XFORM_OPAQUE_CODE
    /* 1C14 80011414 */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 1C18 80011418 */  bnez        $t1, .LtmdGt3PreXformReverseLoop
    /* 1C1C 8001141C */  lw          $a1, 0x84($a0)
    /* 1C20 80011420 */  j           .LtmdGt3PreXformLoop
    /* 1C24 80011424 */  nop
  .LtmdGt3PreXformAdvance:
    /* Rejection consumes the same reserved slot as an accepted triangle. */
    /* 1C28 80011428 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
    /* 1C2C 8001142C */  addu        $a2, $t9, $a2
  .LtmdGt3PreXformLoop:
    /* 1C30 80011430 */  beq         $zero, $a3, .LtmdGt3PreXformReturn
    /* 1C34 80011434 */  nop
    /* Feed the packet's existing corners through SXY's FIFO for the facing test. */
    /* 1C38 80011438 */  lwc2        $15, 0x8($t8)
    /* 1C3C 8001143C */  lwc2        $15, 0x14($t8)
    /* 1C40 80011440 */  lwc2        $15, 0x20($t8)
    /* 1C44 80011444 */  addiu       $a3, $a3, -0x1
    /* 1C48 80011448 */  nop
    /* 1C4C 8001144C */  nclip
    /* 1C50 80011450 */  lw          $t1, 0x0($a2)
    /* 1C54 80011454 */  lw          $t3, 0x4($a2)
    /* 1C58 80011458 */  mfc2        $t0, $24
    /* 1C5C 8001145C */  srl         $t2, $t1, 16
    /* The three halfwords are depth-cache byte offsets, not vertex-array offsets. */
    /* Reject nonpositive winding and failed projections marked with TMD_VERTEX_DEPTH_INVALID. */
    /* 1C60 80011460 */  blez        $t0, .LtmdGt3PreXformAdvance
    /* 1C64 80011464 */  addu        $t2, $t6, $t2
    /* 1C68 80011468 */  lw          $t2, 0x0($t2)
    /* 1C6C 8001146C */  sll         $t1, $t1, 16
    /* 1C70 80011470 */  bltz        $t2, .LtmdGt3PreXformAdvance
    /* 1C74 80011474 */  srl         $t1, $t1, 16
    /* 1C78 80011478 */  addu        $t1, $t6, $t1
    /* 1C7C 8001147C */  lw          $t1, 0x0($t1)
    /* 1C80 80011480 */  sll         $t3, $t3, 16
    /* 1C84 80011484 */  bltz        $t1, .LtmdGt3PreXformAdvance
    /* 1C88 80011488 */  srl         $t3, $t3, 16
    /* 1C8C 8001148C */  addu        $t3, $t6, $t3
    /* 1C90 80011490 */  lw          $t3, 0x0($t3)
    /* 1C94 80011494 */  mtc2        $t1, $17
    /* 1C98 80011498 */  bltz        $t3, .LtmdGt3PreXformAdvance
    /* 1C9C 8001149C */  mtc2        $t2, $18
    /* 1CA0 800114A0 */  mtc2        $t3, $19
    /* Average valid cached depths with the caller's ZSF3 scale, then link the packet. */
    /* 1CA4 800114A4 */  lui         $t0, (TMD_DRAW_STREAM_GT3_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* 1CA8 800114A8 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* 1CAC 800114AC */  avsz3
    /* 1CB0 800114B0 */  and         $t1, $t0, $t8
    /* 1CB4 800114B4 */  mfc2        $t2, $7
    /* 1CB8..1CE0 800114B8..800114E0 */  TMD_DRAW_STREAM_GT3_PRE_XFORM_LINK_PACKET
    /* 1CE4 800114E4 */  j           .LtmdGt3PreXformAdvance
    /* 1CE8 800114E8 */  nop
  .LtmdGt3PreXformReturn:
    /* 1CEC 800114EC */  sw          $t8, 0x4($a0)
    /* 1CF0 800114F0 */  addu        $v0, $zero, $a2
    /* 1CF4 800114F4 */  jr          $ra
    /* 1CF8 800114F8 */  nop
/* Take only the semi-transparent command byte; keep the packet's pre-pass RGB. */
.equ TMD_DRAW_STREAM_GT4_PRE_XFORM_SEMI_TRANS_CODE, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR >> 24)

alabel tmdDrawStreamPrimGt4PreXformSemiTrans
    /* a0 workspace, a1 object flags, a2 first element word; a3 counts locally. */
    /* Force semi-transparency, then select the shared walk's facing rule. */
    /* 1CFC 800114FC */  lw          $t9, 0x18($a0)
    /* 1D00 80011500 */  lw          $a3, 0x1C($a0)
    /* 1D04 80011504 */  lw          $t8, 0x4($a0)
    /* 1D08 80011508 */  lw          $t7, 0x14($a0)
    /* 1D0C 8001150C */  lw          $t6, 0x10($a0)
    /* 1D10 80011510 */  sll         $t9, $t9, 2
    /* 1D14 80011514 */  lui         $v0, (TMD_DRAW_STREAM_GT4_PACKET_WORDS << 8)
    /* 1D18 80011518 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT4_PRE_XFORM_SEMI_TRANS_CODE
    /* 1D1C 8001151C */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 1D20 80011520 */  bnez        $t1, .LtmdGt4PreXformReverseLoop
    /* 1D24 80011524 */  lw          $a1, 0x84($a0)
    /* 1D28 80011528 */  j           .LtmdGt4PreXformLoop
    /* 1D2C 8001152C */  nop

/* Reuse GT4 packet/depth constants; pre-pass RGB remains in the packet. */
.equ TMD_DRAW_STREAM_GT4_PRE_XFORM_OPAQUE_CODE, (TMD_DRAW_STREAM_GT4_COLOR >> 24)
/* GPU linked-list addresses retain the low 24 bits of a packet address. */
.equ TMD_DRAW_STREAM_GT4_PRE_XFORM_DMA_ADDRESS_MASK, 0x00FFFFFF

/*
 * Complete the packet's command/tag and prepend it to its wrapped depth bucket.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ4 OTZ, a1 depth shift, t0 DMA address mask, v1 GPU command byte,
 * v0 twelve-word DMA length in bits 24..31. Clobbers t2 and t5; writes the
 * OT head, packet tag and command byte, retaining all XY, RGB and texture data.
 * Both facing walks and the semi-transparent entry use this expansion.
 * Fixed registers preserve the load/GTE delays; it emits no call or extra
 * instructions and is purged after this body's reverse-culling walk.
 */
.macro TMD_DRAW_STREAM_GT4_PRE_XFORM_LINK_PACKET
    sb          $v1, 0x7($t8)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t5, 0x0($t2)
    sw          $t1, 0x0($t2)
    and         $t5, $t0, $t5
    or          $t5, $v0, $t5
    sw          $t5, 0x0($t8)
.endm

glabel tmdDrawStreamPrimGt4PreXform
    /* a0 workspace, a1 object flags, a2 first element word; a3 counts locally. */
    /* t8 first-region packet cursor, t6 depth cache, t7 displaced OT base. */
    /* 1D30 80011530 */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 1D34 80011534 */  bnez        $t0, tmdDrawStreamPrimGt4PreXformSemiTrans
    /* 1D38 80011538 */  nop
    /* 1D3C 8001153C */  lw          $t9, 0x18($a0)
    /* 1D40 80011540 */  lw          $a3, 0x1C($a0)
    /* 1D44 80011544 */  lw          $t8, 0x4($a0)
    /* 1D48 80011548 */  lw          $t7, 0x14($a0)
    /* 1D4C 8001154C */  lw          $t6, 0x10($a0)
    /* 1D50 80011550 */  sll         $t9, $t9, 2
    /* 1D54 80011554 */  lui         $v0, (TMD_DRAW_STREAM_GT4_PACKET_WORDS << 8)
    /* 1D58 80011558 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT4_PRE_XFORM_OPAQUE_CODE
    /* 1D5C 8001155C */  andi        $t1, $a1, TMD_OBJECT_REVERSE_CULLING
    /* 1D60 80011560 */  bnez        $t1, .LtmdGt4PreXformReverseLoop
    /* 1D64 80011564 */  lw          $a1, 0x84($a0)
    /* 1D68 80011568 */  j           .LtmdGt4PreXformLoop
    /* 1D6C 8001156C */  nop
  .LtmdGt4PreXformAdvance:
    /* Rejection consumes the same reserved slot as an accepted quad. */
    /* 1D70 80011570 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
    /* 1D74 80011574 */  addu        $a2, $t9, $a2
  .LtmdGt4PreXformLoop:
    /* 1D78 80011578 */  beq         $zero, $a3, .LtmdGt4PreXformReturn
    /* 1D7C 8001157C */  nop
    /* Test existing packet corners; the SXY FIFO first holds 0, 1, 2. */
    /* 1D80 80011580 */  lwc2        $15, 0x8($t8)
    /* 1D84 80011584 */  lwc2        $15, 0x14($t8)
    /* 1D88 80011588 */  lwc2        $15, 0x20($t8)
    /* 1D8C 8001158C */  addiu       $a3, $a3, -0x1
    /* 1D90 80011590 */  nop
    /* 1D94 80011594 */  nclip
    /* 1D98 80011598 */  lw          $t1, 0x0($a2)
    /* 1D9C 8001159C */  lw          $t3, 0x4($a2)
    /* 1DA0 800115A0 */  mfc2        $t0, $24
    /* 1DA4 800115A4 */  srl         $t2, $t1, 16
    /* 1DA8 800115A8 */  bgtz        $t0, .LtmdGt4PreXformCheckDepths
    /* 1DAC 800115AC */  nop
    /* The fallback keeps strict negative winding of corners 1, 2, 3. */
    /* 1DB0 800115B0 */  lwc2        $15, 0x2C($t8)
    /* 1DB4 800115B4 */  nop
    /* 1DB8 800115B8 */  nop
    /* 1DBC 800115BC */  nclip
    /* 1DC0 800115C0 */  mfc2        $t0, $24
    /* 1DC4 800115C4 */  nop
    /* 1DC8 800115C8 */  bgez        $t0, .LtmdGt4PreXformAdvance
  .LtmdGt4PreXformCheckDepths:
    /* Halfwords are depth-cache byte offsets; TMD_VERTEX_DEPTH_INVALID marks failed projections. */
    /* 1DCC 800115CC */  addu        $t2, $t6, $t2
    /* 1DD0 800115D0 */  lw          $t2, 0x0($t2)
    /* 1DD4 800115D4 */  sll         $t1, $t1, 16
    /* 1DD8 800115D8 */  bltz        $t2, .LtmdGt4PreXformAdvance
    /* 1DDC 800115DC */  srl         $t1, $t1, 16
    /* 1DE0 800115E0 */  addu        $t1, $t6, $t1
    /* 1DE4 800115E4 */  lw          $t1, 0x0($t1)
    /* 1DE8 800115E8 */  srl         $t4, $t3, 16
    /* 1DEC 800115EC */  bltz        $t1, .LtmdGt4PreXformAdvance
    /* 1DF0 800115F0 */  addu        $t4, $t6, $t4
    /* 1DF4 800115F4 */  lw          $t4, 0x0($t4)
    /* 1DF8 800115F8 */  sll         $t3, $t3, 16
    /* 1DFC 800115FC */  bltz        $t4, .LtmdGt4PreXformAdvance
    /* 1E00 80011600 */  srl         $t3, $t3, 16
    /* 1E04 80011604 */  addu        $t3, $t6, $t3
    /* 1E08 80011608 */  lw          $t3, 0x0($t3)
    /* 1E0C 8001160C */  mtc2        $t1, $16
    /* 1E10 80011610 */  bltz        $t3, .LtmdGt4PreXformAdvance
    /* 1E14 80011614 */  mtc2        $t2, $17
    /* 1E18 80011618 */  mtc2        $t3, $18
    /* 1E1C 8001161C */  mtc2        $t4, $19
    /* Average valid cached depths with the caller's ZSF4 scale, then link the packet. */
    /* 1E20 80011620 */  lui         $t0, (TMD_DRAW_STREAM_GT4_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* 1E24 80011624 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* 1E28 80011628 */  avsz4
    /* 1E2C 8001162C */  and         $t1, $t0, $t8
    /* 1E30 80011630 */  mfc2        $t2, $7
    /* 1E34..1E5C 80011634..8001165C */  TMD_DRAW_STREAM_GT4_PRE_XFORM_LINK_PACKET
    /* 1E60 80011660 */  j           .LtmdGt4PreXformAdvance
    /* 1E64 80011664 */  nop
  .LtmdGt4PreXformReturn:
    /* 1E68 80011668 */  sw          $t8, 0x4($a0)
    /* 1E6C 8001166C */  addu        $v0, $zero, $a2
    /* 1E70 80011670 */  jr          $ra
    /* 1E74 80011674 */  nop
/* POLY_G3 includes its four-byte DMA tag; the GPU length excludes it. */
.equ TMD_DRAW_STREAM_PRIM_G3_PACKET_BYTES, 28
.equ TMD_DRAW_STREAM_PRIM_G3_PACKET_WORDS, (TMD_DRAW_STREAM_PRIM_G3_PACKET_BYTES / 4) - 1
/* Wrap the scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_PRIM_G3_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_PRIM_G3_DEPTH_SHIFT, 4

/*
 * Prepend the packet to its depth bucket and repeat the face colour at each corner.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 OTZ, a1 depth shift, v0 six-word DMA length in bits 24..31,
 * GTE RGB2 lit face RGB/command word. Clobbers t0, t2 and t3; writes the
 * OT head, packet tag and three colour words. Emits no call or extra delays.
 */
.macro TMD_DRAW_STREAM_PRIM_G3_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_PRIM_G3_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_PRIM_G3_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, 8
    srl         $t0, $t0, 8
    or          $t0, $v0, $t0
    mfc2        $t3, $22
    sw          $t0, 0x0($t8)
    sw          $t3, 0x4($t8)
    sw          $t3, 0xC($t8)
    sw          $t3, 0x14($t8)
.endm

glabel tmdDrawStreamPrimG3
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* 1E78 80011678 */  lw          $t9, 0x18($a0)
    /* 1E7C 8001167C */  lw          $a3, 0x1C($a0)
    /* 1E80 80011680 */  lw          $t8, 0x0($a0)
    /* 1E84 80011684 */  lw          $t7, 0x14($a0)
    /* 1E88 80011688 */  lw          $t6, 0x8($a0)
    /* 1E8C 8001168C */  lw          $t5, 0xC($a0)
    /* 1E90 80011690 */  sll         $t9, $t9, 2
    /* 1E94 80011694 */  lui         $v0, (TMD_DRAW_STREAM_PRIM_G3_PACKET_WORDS << 8)
    /* 1E98 80011698 */  j           .L800116A8
    /* 1E9C 8001169C */  lw          $a1, 0x84($a0)
  .L800116A0:
    /* Rejection consumes the same reserved slot as a drawable triangle. */
    /* 1EA0 800116A0 */  addiu       $t8, $t8, TMD_DRAW_STREAM_PRIM_G3_PACKET_BYTES
  .L800116A4:
    /* 1EA4 800116A4 */  addu        $a2, $t9, $a2
  .L800116A8:
    /* 1EA8 800116A8 */  beq         $zero, $a3, .L800117AC
    /* 1EAC 800116AC */  nop
    /* 1EB0 800116B0 */  addiu       $a3, $a3, -0x1
    /* Unpack three vertex byte offsets and one face-normal byte offset. */
    /* 1EB4 800116B4 */  lw          $t1, 0x0($a2)
    /* 1EB8 800116B8 */  lw          $t3, 0x4($a2)
    /* 1EBC 800116BC */  srl         $t2, $t1, 16
    /* 1EC0 800116C0 */  addu        $t2, $t6, $t2
    /* 1EC4 800116C4 */  sll         $t1, $t1, 16
    /* 1EC8 800116C8 */  srl         $t1, $t1, 16
    /* 1ECC 800116CC */  addu        $t1, $t6, $t1
    /* 1ED0 800116D0 */  srl         $t4, $t3, 16
    /* 1ED4 800116D4 */  sll         $t3, $t3, 16
    /* 1ED8 800116D8 */  srl         $t3, $t3, 16
    /* 1EDC 800116DC */  addu        $t3, $t6, $t3
    /* 1EE0 800116E0 */  lwc2        $0, 0x0($t1)
    /* 1EE4 800116E4 */  lwc2        $1, 0x4($t1)
    /* 1EE8 800116E8 */  lwc2        $2, 0x0($t2)
    /* 1EEC 800116EC */  lwc2        $3, 0x4($t2)
    /* 1EF0 800116F0 */  lwc2        $4, 0x0($t3)
    /* 1EF4 800116F4 */  lwc2        $5, 0x4($t3)
    /* 1EF8 800116F8 */  sll         $t1, $t4, 0
    /* 1EFC 800116FC */  nop
    /* Project all corners (RTPT), then reject GTE errors and nonpositive winding. */
    /* 1F00 80011700 */  .word 0x4A280030 /* RTPT */
    /* 1F04 80011704 */  addu        $t1, $t5, $t1
    /* 1F08 80011708 */  cfc2        $t0, $31
    /* 1F0C 8001170C */  nop
    /* 1F10 80011710 */  bltz        $t0, .L800116A0
    /* 1F14 80011714 */  nop
    /* 1F18 80011718 */  nop
    /* 1F1C 8001171C */  nop
    /* 1F20 80011720 */  .word 0x4B400006 /* NCLIP */
    /* 1F24 80011724 */  mfc2        $t0, $24
    /* 1F28 80011728 */  nop
    /* 1F2C 8001172C */  blez        $t0, .L800116A0
    /* 1F30 80011730 */  nop
    /* 1F34 80011734 */  swc2        $12, 0x8($t8)
    /* 1F38 80011738 */  swc2        $13, 0x10($t8)
    /* 1F3C 8001173C */  swc2        $14, 0x18($t8)
    /* 1F40 80011740 */  nop
    /* 1F44 80011744 */  nop
    /* Average depth (AVSZ3) and light the element colour from its one normal (NCCS). */
    /* 1F48 80011748 */  .word 0x4B58002D /* AVSZ3 */
    /* 1F4C 8001174C */  lwc2        $6, 0x8($a2)
    /* 1F50 80011750 */  lwc2        $0, 0x0($t1)
    /* 1F54 80011754 */  lwc2        $1, 0x4($t1)
    /* 1F58 80011758 */  mfc2        $t2, $7
    /* 1F5C 8001175C */  sll         $t1, $t8, 8
    /* 1F60 80011760 */  srl         $t1, $t1, 8
    /* 1F64 80011764 */  .word 0x4B08041B /* NCCS */
    /* 1F68..1FA0 80011768..800117A0 */  TMD_DRAW_STREAM_PRIM_G3_LINK_PACKET
    /* 1FA4 800117A4 */  j           .L800116A0
    /* 1FA8 800117A8 */  nop
  .L800117AC:
    /* 1FAC 800117AC */  sw          $t8, 0x0($a0)
  .L800117B0:
    /* 1FB0 800117B0 */  addu        $v0, $zero, $a2
    /* 1FB4 800117B4 */  jr          $ra
    /* 1FB8 800117B8 */  nop
.purgem TMD_DRAW_STREAM_PRIM_G3_LINK_PACKET

/* POLY_G4 includes its DMA tag; the GPU length excludes that word. */
.equ TMD_DRAW_STREAM_PRIM_G4_PACKET_BYTES, 36
.equ TMD_DRAW_STREAM_PRIM_G4_PACKET_WORDS, (TMD_DRAW_STREAM_PRIM_G4_PACKET_BYTES / 4) - 1
/* Wrap scaled depth to 14 bits, then quantize by 16 into 1024 OT buckets. */
.equ TMD_DRAW_STREAM_PRIM_G4_DEPTH_MASK, 0x3FFF
.equ TMD_DRAW_STREAM_PRIM_G4_DEPTH_SHIFT, 4
.equ TMD_DRAW_STREAM_PRIM_G4_DMA_ADDRESS_BITS, 24
/* Outside the unsigned u16 vertex-offset domain; forces a fresh projection. */
.equ TMD_DRAW_STREAM_PRIM_G4_NO_PREVIOUS_VERTEX, 0x0FFF0000

/*
 * Prepend a G4 packet and repeat the lit face colour at all four corners.
 * Inputs: t8 packet, t7 displaced OT base, a1 depth shift, GTE OTZ from
 * AVSZ4 and pending RGB2 from NCCS. Clobbers t0..t3; writes the OT head,
 * eight-word DMA tag and four RGB/command words. Preserves v0 (vertex 0 XY)
 * and v1 (vertex 0 offset) for the next element's vertex-3 reuse.
 * Fixed registers belong to this handler. Link arithmetic overlaps NCCS;
 * preserve the instruction schedule without adding calls or delays.
 */
.macro TMD_DRAW_STREAM_PRIM_G4_LINK_PACKET
    sll         $t1, $t8, (32 - TMD_DRAW_STREAM_PRIM_G4_DMA_ADDRESS_BITS)
    mfc2        $t2, $7
    srl         $t1, $t1, (32 - TMD_DRAW_STREAM_PRIM_G4_DMA_ADDRESS_BITS)
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_PRIM_G4_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_PRIM_G4_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_PRIM_G4_DMA_ADDRESS_BITS)
    lui         $t1, (TMD_DRAW_STREAM_PRIM_G4_PACKET_WORDS << 8)
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_PRIM_G4_DMA_ADDRESS_BITS)
    or          $t0, $t1, $t0
    mfc2        $t3, $22
    sw          $t0, 0x0($t8)
    sw          $t3, 0x4($t8)
    sw          $t3, 0xC($t8)
    sw          $t3, 0x14($t8)
    sw          $t3, 0x1C($t8)
.endm

glabel tmdDrawStreamPrimG4
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* 1FBC 800117BC */  lw          $t9, 0x18($a0)
    /* 1FC0 800117C0 */  lw          $a3, 0x1C($a0)
    /* 1FC4 800117C4 */  lw          $t8, 0x0($a0)
    /* 1FC8 800117C8 */  lw          $t7, 0x14($a0)
    /* 1FCC 800117CC */  lw          $t6, 0x8($a0)
    /* 1FD0 800117D0 */  lw          $t5, 0xC($a0)
    /* 1FD4 800117D4 */  sll         $t9, $t9, 2
    /* 1FD8 800117D8 */  lui         $v1, (TMD_DRAW_STREAM_PRIM_G4_NO_PREVIOUS_VERTEX >> 16)
    /* 1FDC 800117DC */  j           .LtmdG4Loop
    /* 1FE0 800117E0 */  lw          $a1, 0x84($a0)
  .LtmdG4ProjectionRejected:
    /* A failed projection must not seed the next element's reuse. */
    /* 1FE4 800117E4 */  lui         $v1, (TMD_DRAW_STREAM_PRIM_G4_NO_PREVIOUS_VERTEX >> 16)
  .LtmdG4Advance:
    /* Every element consumes a packet slot, even when rejected. */
    /* 1FE8 800117E8 */  addiu       $t8, $t8, TMD_DRAW_STREAM_PRIM_G4_PACKET_BYTES
    /* 1FEC 800117EC */  addu        $a2, $t9, $a2
  .LtmdG4Loop:
    /* 1FF0 800117F0 */  beq         $zero, $a3, .LtmdG4Done
    /* 1FF4 800117F4 */  nop
    /* 1FF8 800117F8 */  addiu       $a3, $a3, -0x1
    /* Project vertex 3 first, unless the preceding vertex 0 supplies XY and SZ. */
    /* 1FFC 800117FC */  lw          $t3, 0x4($a2)
    /* 2000 80011800 */  lw          $t1, 0x0($a2)
    /* 2004 80011804 */  srl         $t4, $t3, 16
    /* 2008 80011808 */  beq         $t4, $v1, .LtmdG4ReuseVertex3
    /* 200C 8001180C */  addu        $t4, $t6, $t4
    /* 2010 80011810 */  lwc2        $0, 0x0($t4)
    /* 2014 80011814 */  lwc2        $1, 0x4($t4)
    /* 2018 80011818 */  sll         $t3, $t3, 16
    /* 201C 8001181C */  srl         $t3, $t3, 16
    /* 2020 80011820 */  rtps
    /* 2024 80011824 */  addu        $t3, $t6, $t3
    /* 2028 80011828 */  srl         $t2, $t1, 16
    /* 202C 8001182C */  addu        $t2, $t6, $t2
    /* 2030 80011830 */  sll         $t1, $t1, 16
    /* 2034 80011834 */  srl         $t1, $t1, 16
    /* 2038 80011838 */  addu        $v1, $t1, $zero
    /* 203C 8001183C */  addu        $t1, $t6, $t1
    /* 2040 80011840 */  cfc2        $t0, $31
    /* 2044 80011844 */  nop
    /* 2048 80011848 */  bltz        $t0, .LtmdG4ProjectionRejected
    /* 204C 8001184C */  nop
    /* 2050 80011850 */  swc2        $14, 0x20($t8)
    /* 2054 80011854 */  j           .LtmdG4ProjectOtherCorners
    /* 2058 80011858 */  nop
  .LtmdG4ReuseVertex3:
    /* 205C 8001185C */  sll         $t3, $t3, 16
    /* 2060 80011860 */  srl         $t3, $t3, 16
    /* 2064 80011864 */  addu        $t3, $t6, $t3
    /* 2068 80011868 */  srl         $t2, $t1, 16
    /* 206C 8001186C */  addu        $t2, $t6, $t2
    /* 2070 80011870 */  sll         $t1, $t1, 16
    /* 2074 80011874 */  srl         $t1, $t1, 16
    /* 2078 80011878 */  addu        $v1, $t1, $zero
    /* 207C 8001187C */  addu        $t1, $t6, $t1
    /* 2080 80011880 */  sw          $v0, 0x20($t8)
  .LtmdG4ProjectOtherCorners:
    /* RTPT(2,1,0) shifts vertex 3's depth into SZ0 for the later AVSZ4. */
    /* 2084 80011884 */  lwc2        $0, 0x0($t3)
    /* 2088 80011888 */  lwc2        $1, 0x4($t3)
    /* 208C 8001188C */  lwc2        $2, 0x0($t2)
    /* 2090 80011890 */  lwc2        $3, 0x4($t2)
    /* 2094 80011894 */  lwc2        $4, 0x0($t1)
    /* 2098 80011898 */  lwc2        $5, 0x4($t1)
    /* 209C 8001189C */  nop
    /* 20A0 800118A0 */  nop
    /* 20A4 800118A4 */  rtpt
    /* The face-normal reference uses all 32 bits of the byte-offset word. */
    /* 20A8 800118A8 */  lw          $t1, 0x8($a2)
    /* 20AC 800118AC */  nop
    /* 20B0 800118B0 */  addu        $t1, $t5, $t1
    /* 20B4 800118B4 */  cfc2        $t0, $31
    /* 20B8 800118B8 */  nop
    /* 20BC 800118BC */  bltz        $t0, .LtmdG4ProjectionRejected
    /* 20C0 800118C0 */  nop
    /* Keep a quad if either triangle faces forward; preserve vertex 0 XY for reuse. */
    /* 20C4 800118C4 */  nclip
    /* 20C8 800118C8 */  mfc2        $v0, $14
    /* 20CC 800118CC */  swc2        $12, 0x18($t8)
    /* 20D0 800118D0 */  swc2        $13, 0x10($t8)
    /* 20D4 800118D4 */  sw          $v0, 0x8($t8)
    /* 20D8 800118D8 */  mfc2        $t0, $24
    /* 20DC 800118DC */  nop
    /* 20E0 800118E0 */  bltz        $t0, .LtmdG4LightAndLink
    /* 20E4 800118E4 */  nop
    /* 20E8 800118E8 */  lwc2        $14, 0x20($t8)
    /* 20EC 800118EC */  nop
    /* 20F0 800118F0 */  nop
    /* 20F4 800118F4 */  nclip
    /* 20F8 800118F8 */  mfc2        $t0, $24
    /* 20FC 800118FC */  nop
    /* 2100 80011900 */  blez        $t0, .LtmdG4Advance
    /* 2104 80011904 */  nop
  .LtmdG4LightAndLink:
    /* 2108 80011908 */  nop
    /* 210C 8001190C */  nop
    /* Average all four depths and light the element's material with its face normal. */
    /* 2110 80011910 */  avsz4
    /* 2114 80011914 */  lwc2        $6, 0xC($a2)
    /* 2118 80011918 */  lwc2        $0, 0x0($t1)
    /* 211C 8001191C */  lwc2        $1, 0x4($t1)
    /* 2120 80011920 */  nop
    /* 2124 80011924 */  nop
    /* 2128 80011928 */  nccs
    /* 212C..2178 8001192C..80011978 */  TMD_DRAW_STREAM_PRIM_G4_LINK_PACKET
    /* 217C 8001197C */  j           .LtmdG4Advance
    /* 2180 80011980 */  nop
  .LtmdG4Done:
    /* 2184 80011984 */  sw          $t8, 0x0($a0)
    /* 2188 80011988 */  addu        $v0, $zero, $a2
    /* 218C 8001198C */  jr          $ra
    /* 2190 80011990 */  nop
endlabel tmdDrawStreamPrimG4
.purgem TMD_DRAW_STREAM_PRIM_G4_LINK_PACKET
alabel tmdDrawStreamPrimGt3OneNormalSemiTrans
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* Seed the semi-transparent command and neutral material before the shared face-normal walk. */
    /* 2194 80011994 */  lui         $t0, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR >> 16)
    /* 2198 80011998 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR & 0xFFFF)
    /* 219C 8001199C */  mtc2        $t0, $6
    /* 21A0 800119A0 */  j           .LtmdGt3OneNormalSetup
    /* 21A4 800119A4 */  nop

/* DMA tags retain 24 address bits; reuse the GT3 packet, material and depth constants. */
.equ TMD_DRAW_STREAM_GT3_ONE_NORMAL_DMA_ADDRESS_BITS, 24

/*
 * Complete and prepend a GT3 packet with one lit colour for all corners.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ3 OTZ, a1 depth shift, v0 nine-word DMA length in bits 24..31,
 * GTE RGB2 from the preceding NCCS. Clobbers t0, t2 and t3; writes the
 * OT head, packet tag and three colour words, leaving texture words intact.
 * The link arithmetic runs while NCCS completes; retain the colour read's
 * load delay before its stores. This fixed-register expansion emits no call.
 */
.macro TMD_DRAW_STREAM_GT3_ONE_NORMAL_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_GT3_ONE_NORMAL_DMA_ADDRESS_BITS)
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_GT3_ONE_NORMAL_DMA_ADDRESS_BITS)
    or          $t0, $v0, $t0
    mfc2        $t3, $22
    sw          $t0, 0x0($t8)
    sw          $t3, 0x4($t8)
    sw          $t3, 0x10($t8)
    sw          $t3, 0x1C($t8)
.endm

glabel tmdDrawStreamPrimGt3OneNormal
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* 21A8 800119A8 */  lui         $t0, (TMD_DRAW_STREAM_GT3_COLOR >> 16)
    /* 21AC 800119AC */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_COLOR & 0xFFFF)
    /* 21B0 800119B0 */  mtc2        $t0, $6
  .LtmdGt3OneNormalSetup:
    /* 21B4 800119B4 */  lw          $t9, 0x18($a0)
    /* 21B8 800119B8 */  lw          $a3, 0x1C($a0)
    /* 21BC 800119BC */  lw          $t8, 0x0($a0)
    /* 21C0 800119C0 */  lw          $t7, 0x14($a0)
    /* 21C4 800119C4 */  lw          $t6, 0x8($a0)
    /* 21C8 800119C8 */  lw          $t5, 0xC($a0)
    /* 21CC 800119CC */  sll         $t9, $t9, 2
    /* 21D0 800119D0 */  lui         $v0, (TMD_DRAW_STREAM_GT3_PACKET_WORDS << 8)
    /* 21D4 800119D4 */  j           .LtmdGt3OneNormalLoop
    /* 21D8 800119D8 */  lw          $a1, 0x84($a0)
  .LtmdGt3OneNormalAdvance:
    /* Rejection consumes the same packet slot as an accepted triangle. */
    /* 21DC 800119DC */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
    /* 21E0 800119E0 */  addu        $a2, $t9, $a2
  .LtmdGt3OneNormalLoop:
    /* 21E4 800119E4 */  beq         $zero, $a3, .LtmdGt3OneNormalDone
    /* 21E8 800119E8 */  nop
    /* 21EC 800119EC */  addiu       $a3, $a3, -0x1
    /* Unpack three vertex byte offsets and the face normal's byte offset. */
    /* 21F0 800119F0 */  lw          $t1, 0x0($a2)
    /* 21F4 800119F4 */  lw          $t3, 0x4($a2)
    /* 21F8 800119F8 */  srl         $t2, $t1, 16
    /* 21FC 800119FC */  addu        $t2, $t6, $t2
    /* 2200 80011A00 */  sll         $t1, $t1, 16
    /* 2204 80011A04 */  srl         $t1, $t1, 16
    /* 2208 80011A08 */  addu        $t1, $t6, $t1
    /* 220C 80011A0C */  srl         $t4, $t3, 16
    /* 2210 80011A10 */  sll         $t3, $t3, 16
    /* 2214 80011A14 */  srl         $t3, $t3, 16
    /* 2218 80011A18 */  addu        $t3, $t6, $t3
    /* 221C 80011A1C */  lwc2        $0, 0x0($t1)
    /* 2220 80011A20 */  lwc2        $1, 0x4($t1)
    /* 2224 80011A24 */  lwc2        $2, 0x0($t2)
    /* 2228 80011A28 */  lwc2        $3, 0x4($t2)
    /* 222C 80011A2C */  lwc2        $4, 0x0($t3)
    /* 2230 80011A30 */  lwc2        $5, 0x4($t3)
    /* 2234 80011A34 */  nop
    /* 2238 80011A38 */  sll         $t1, $t4, 0
    /* Project all corners, then reject GTE errors and nonpositive winding. */
    /* 223C 80011A3C */  rtpt
    /* 2240 80011A40 */  addu        $t1, $t5, $t1
    /* 2244 80011A44 */  cfc2        $t0, $31
    /* 2248 80011A48 */  nop
    /* 224C 80011A4C */  bltz        $t0, .LtmdGt3OneNormalAdvance
    /* 2250 80011A50 */  nop
    /* 2254 80011A54 */  nop
    /* 2258 80011A58 */  nop
    /* 225C 80011A5C */  nclip
    /* 2260 80011A60 */  mfc2        $t0, $24
    /* 2264 80011A64 */  nop
    /* 2268 80011A68 */  blez        $t0, .LtmdGt3OneNormalAdvance
    /* 226C 80011A6C */  nop
    /* 2270 80011A70 */  swc2        $12, 0x8($t8)
    /* 2274 80011A74 */  swc2        $13, 0x14($t8)
    /* 2278 80011A78 */  swc2        $14, 0x20($t8)
    /* 227C 80011A7C */  nop
    /* 2280 80011A80 */  nop
    /* Average the three depths and light neutral RGB with the face normal. */
    /* 2284 80011A84 */  avsz3
    /* 2288 80011A88 */  lwc2        $0, 0x0($t1)
    /* 228C 80011A8C */  lwc2        $1, 0x4($t1)
    /* 2290 80011A90 */  mfc2        $t2, $7
    /* 2294 80011A94 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT3_ONE_NORMAL_DMA_ADDRESS_BITS)
    /* 2298 80011A98 */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT3_ONE_NORMAL_DMA_ADDRESS_BITS)
    /* 229C 80011A9C */  nccs
    /* 22A0..22D8 80011AA0..80011AD8 */  TMD_DRAW_STREAM_GT3_ONE_NORMAL_LINK_PACKET
    /* 22DC 80011ADC */  j           .LtmdGt3OneNormalAdvance
    /* 22E0 80011AE0 */  nop
  .LtmdGt3OneNormalDone:
    /* 22E4 80011AE4 */  sw          $t8, 0x0($a0)
    /* 22E8 80011AE8 */  addu        $v0, $zero, $a2
    /* 22EC 80011AEC */  jr          $ra
    /* 22F0 80011AF0 */  nop
.purgem TMD_DRAW_STREAM_GT3_ONE_NORMAL_LINK_PACKET

/*
 * Complete and prepend a GT4 packet with one lit colour for all four corners.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ4 OTZ, a1 depth shift, GTE RGB2 from the preceding NCCS.
 * Clobbers t0, t1, t2 and t3; writes the OT head, packet tag and four colour
 * words, leaving texture words and v0/v1's vertex-0 XY/reuse key intact.
 * Keep the link arithmetic during NCCS and the tag store after the colour
 * read: these instructions supply the GTE and load delays. No call is emitted.
 */
.macro TMD_DRAW_STREAM_GT4_ONE_NORMAL_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    lui         $t1, (TMD_DRAW_STREAM_GT4_PACKET_WORDS << (TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS - 16))
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    or          $t0, $t1, $t0
    mfc2        $t3, $22
    sw          $t0, 0x0($t8)
    sw          $t3, 0x4($t8)
    sw          $t3, 0x10($t8)
    sw          $t3, 0x1C($t8)
    sw          $t3, 0x28($t8)
.endm

alabel tmdDrawStreamPrimGt4OneNormal
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* Seed the opaque command and neutral material for the shared face-normal walk. */
    /* 22F4 80011AF4 */  lui         $t0, (TMD_DRAW_STREAM_GT4_COLOR >> 16)
    /* 22F8 80011AF8 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_COLOR & 0xFFFF)
    /* 22FC 80011AFC */  mtc2        $t0, $6
    /* 2300 80011B00 */  j           .LtmdGt4OneNormalSetup
    /* 2304 80011B04 */  nop
glabel tmdDrawStreamPrimGt4OneNormalSemiTrans
    /* a0 workspace, a1 ignored object flags, a2 first element word. */
    /* Seed the semi-transparent command and neutral material for the shared face-normal walk. */
    /* 2308 80011B08 */  lui         $t0, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR >> 16)
    /* 230C 80011B0C */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR & 0xFFFF)
    /* 2310 80011B10 */  mtc2        $t0, $6
  .LtmdGt4OneNormalSetup:
    /* 2314 80011B14 */  lw          $t9, 0x18($a0)
    /* 2318 80011B18 */  lw          $a3, 0x1C($a0)
    /* 231C 80011B1C */  lw          $t8, 0x0($a0)
    /* 2320 80011B20 */  lw          $t7, 0x14($a0)
    /* 2324 80011B24 */  lw          $t6, 0x8($a0)
    /* 2328 80011B28 */  lw          $t5, 0xC($a0)
    /* 232C 80011B2C */  sll         $t9, $t9, 2
    /* 2330 80011B30 */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
    /* 2334 80011B34 */  j           .LtmdGt4OneNormalLoop
    /* 2338 80011B38 */  lw          $a1, 0x84($a0)
  .LtmdGt4OneNormalProjectionFailed:
    /* Failed projections cannot supply the next element's reused corner. */
    /* 233C 80011B3C */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
  .LtmdGt4OneNormalAdvance:
    /* Rejection consumes the same packet slot as an accepted quad. */
    /* 2340 80011B40 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
    /* 2344 80011B44 */  addu        $a2, $t9, $a2
  .LtmdGt4OneNormalLoop:
    /* 2348 80011B48 */  beq         $zero, $a3, .LtmdGt4OneNormalDone
    /* 234C 80011B4C */  nop
    /* 2350 80011B50 */  addiu       $a3, $a3, -0x1
    /* Project corner 3, or reuse the previous corner 0's XY and FIFO depth. */
    /* 2354 80011B54 */  lw          $t3, 0x4($a2)
    /* 2358 80011B58 */  lw          $t1, 0x0($a2)
    /* 235C 80011B5C */  srl         $t4, $t3, 16
    /* 2360 80011B60 */  beq         $t4, $v1, .LtmdGt4OneNormalReuseCorner
    /* 2364 80011B64 */  addu        $t4, $t6, $t4
    /* 2368 80011B68 */  lwc2        $0, 0x0($t4)
    /* 236C 80011B6C */  lwc2        $1, 0x4($t4)
    /* 2370 80011B70 */  sll         $t3, $t3, 16
    /* 2374 80011B74 */  srl         $t3, $t3, 16
    /* 2378 80011B78 */  rtps
    /* 237C 80011B7C */  addu        $t3, $t6, $t3
    /* 2380 80011B80 */  srl         $t2, $t1, 16
    /* 2384 80011B84 */  addu        $t2, $t6, $t2
    /* 2388 80011B88 */  sll         $t1, $t1, 16
    /* 238C 80011B8C */  srl         $t1, $t1, 16
    /* 2390 80011B90 */  addu        $v1, $t1, $zero
    /* 2394 80011B94 */  addu        $t1, $t6, $t1
    /* 2398 80011B98 */  cfc2        $t0, $31
    /* 239C 80011B9C */  nop
    /* FLAG's signed error summary is independent of the later winding tests. */
    /* 23A0 80011BA0 */  bltz        $t0, .LtmdGt4OneNormalProjectionFailed
    /* 23A4 80011BA4 */  nop
    /* 23A8 80011BA8 */  swc2        $14, 0x2C($t8)
    /* 23AC 80011BAC */  j           .LtmdGt4OneNormalProjectSharedCorners
    /* 23B0 80011BB0 */  nop
  .LtmdGt4OneNormalReuseCorner:
    /* 23B4 80011BB4 */  sll         $t3, $t3, 16
    /* 23B8 80011BB8 */  srl         $t3, $t3, 16
    /* 23BC 80011BBC */  addu        $t3, $t6, $t3
    /* 23C0 80011BC0 */  srl         $t2, $t1, 16
    /* 23C4 80011BC4 */  addu        $t2, $t6, $t2
    /* 23C8 80011BC8 */  sll         $t1, $t1, 16
    /* 23CC 80011BCC */  srl         $t1, $t1, 16
    /* 23D0 80011BD0 */  addu        $v1, $t1, $zero
    /* 23D4 80011BD4 */  addu        $t1, $t6, $t1
    /* 23D8 80011BD8 */  sw          $v0, 0x2C($t8)
  .LtmdGt4OneNormalProjectSharedCorners:
    /* Reverse the shared triangle for projection, retaining element order in the packet. */
    /* 23DC 80011BDC */  lwc2        $0, 0x0($t3)
    /* 23E0 80011BE0 */  lwc2        $1, 0x4($t3)
    /* 23E4 80011BE4 */  lwc2        $2, 0x0($t2)
    /* 23E8 80011BE8 */  lwc2        $3, 0x4($t2)
    /* 23EC 80011BEC */  lwc2        $4, 0x0($t1)
    /* 23F0 80011BF0 */  lwc2        $5, 0x4($t1)
    /* 23F4 80011BF4 */  nop
    /* 23F8 80011BF8 */  nop
    /* 23FC 80011BFC */  rtpt
    /* 2400 80011C00 */  lw          $t1, 0x8($a2)
    /* 2404 80011C04 */  nop
    /* 2408 80011C08 */  addu        $t1, $t5, $t1
    /* 240C 80011C0C */  cfc2        $t0, $31
    /* 2410 80011C10 */  nop
    /* 2414 80011C14 */  bltz        $t0, .LtmdGt4OneNormalProjectionFailed
    /* 2418 80011C18 */  nop
    /* Keep either facing half; even a facing rejection preserves corner 0 for reuse. */
    /* 241C 80011C1C */  nclip
    /* 2420 80011C20 */  mfc2        $v0, $14
    /* 2424 80011C24 */  swc2        $12, 0x20($t8)
    /* 2428 80011C28 */  swc2        $13, 0x14($t8)
    /* 242C 80011C2C */  sw          $v0, 0x8($t8)
    /* 2430 80011C30 */  mfc2        $t0, $24
    /* 2434 80011C34 */  nop
    /* 2438 80011C38 */  bltz        $t0, .LtmdGt4OneNormalDraw
    /* 243C 80011C3C */  nop
    /* 2440 80011C40 */  lwc2        $14, 0x2C($t8)
    /* 2444 80011C44 */  nop
    /* 2448 80011C48 */  nop
    /* 244C 80011C4C */  nclip
    /* 2450 80011C50 */  mfc2        $t0, $24
    /* 2454 80011C54 */  nop
    /* 2458 80011C58 */  blez        $t0, .LtmdGt4OneNormalAdvance
    /* 245C 80011C5C */  nop
  .LtmdGt4OneNormalDraw:
    /* 2460 80011C60 */  nop
    /* 2464 80011C64 */  nop
    /* Average all four depths, then light once from the unmasked normal byte offset. */
    /* 2468 80011C68 */  avsz4
    /* 246C 80011C6C */  lwc2        $0, 0x0($t1)
    /* 2470 80011C70 */  lwc2        $1, 0x4($t1)
    /* 2474 80011C74 */  nop
    /* 2478 80011C78 */  nop
    /* 247C 80011C7C */  nccs
    /* 2480 80011C80 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 2484 80011C84 */  mfc2        $t2, $7
    /* 2488 80011C88 */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 248C..24CC 80011C8C..80011CCC */  TMD_DRAW_STREAM_GT4_ONE_NORMAL_LINK_PACKET
    /* 24D0 80011CD0 */  j           .LtmdGt4OneNormalAdvance
    /* 24D4 80011CD4 */  nop
  .LtmdGt4OneNormalDone:
    /* 24D8 80011CD8 */  sw          $t8, 0x0($a0)
    /* 24DC 80011CDC */  addu        $v0, $zero, $a2
    /* 24E0 80011CE0 */  jr          $ra
    /* 24E4 80011CE4 */  nop
.purgem TMD_DRAW_STREAM_GT4_ONE_NORMAL_LINK_PACKET
    /* 24E8 80011CE8 */  lw          $t9, 0x18($a0)
    /* 24EC 80011CEC */  lw          $a3, 0x1C($a0)
    /* 24F0 80011CF0 */  lw          $t8, 0x0($a0)
    /* 24F4 80011CF4 */  lw          $t7, 0x14($a0)
    /* 24F8 80011CF8 */  lw          $t6, 0x8($a0)
    /* 24FC 80011CFC */  lw          $t5, 0xC($a0)
    /* 2500 80011D00 */  sll         $t9, $t9, 2
    /* 2504 80011D04 */  lui         $v0, 0x800
    /* 2508 80011D08 */  j           .L80011D18
    /* 250C 80011D0C */  lw          $a1, 0x84($a0)
  .L80011D10:
    /* 2510 80011D10 */  addiu       $t8, $t8, 0x24
  .L80011D14:
    /* 2514 80011D14 */  addu        $a2, $t9, $a2
  .L80011D18:
    /* 2518 80011D18 */  beq         $zero, $a3, .L80011E80
    /* 251C 80011D1C */  nop
    /* 2520 80011D20 */  addiu       $a3, $a3, -0x1
    /* 2524 80011D24 */  lw          $t1, 0x0($a2)
    /* 2528 80011D28 */  lw          $t3, 0x4($a2)
    /* 252C 80011D2C */  srl         $t2, $t1, 16
    /* 2530 80011D30 */  addu        $t2, $t6, $t2
    /* 2534 80011D34 */  sll         $t1, $t1, 16
    /* 2538 80011D38 */  srl         $t1, $t1, 16
    /* 253C 80011D3C */  addu        $t1, $t6, $t1
    /* 2540 80011D40 */  srl         $t4, $t3, 16
    /* 2544 80011D44 */  sll         $t3, $t3, 16
    /* 2548 80011D48 */  srl         $t3, $t3, 16
    /* 254C 80011D4C */  addu        $t3, $t6, $t3
    /* 2550 80011D50 */  lwc2        $0, 0x0($t1)
    /* 2554 80011D54 */  lwc2        $1, 0x4($t1)
    /* 2558 80011D58 */  lwc2        $2, 0x0($t2)
    /* 255C 80011D5C */  lwc2        $3, 0x4($t2)
    /* 2560 80011D60 */  lwc2        $4, 0x0($t3)
    /* 2564 80011D64 */  lwc2        $5, 0x4($t3)
    /* 2568 80011D68 */  lw          $t1, 0x8($a2)
    /* 256C 80011D6C */  lw          $t3, 0xC($a2)
    /* 2570 80011D70 */  .word 0x4A280030
    /* 2574 80011D74 */  addu        $t4, $t6, $t4
    /* 2578 80011D78 */  lwc2        $0, 0x0($t4)
    /* 257C 80011D7C */  lwc2        $1, 0x4($t4)
    /* 2580 80011D80 */  srl         $t2, $t1, 16
    /* 2584 80011D84 */  addu        $t2, $t5, $t2
    /* 2588 80011D88 */  sll         $t1, $t1, 16
    /* 258C 80011D8C */  srl         $t1, $t1, 16
    /* 2590 80011D90 */  addu        $t1, $t5, $t1
    /* 2594 80011D94 */  srl         $t4, $t3, 16
    /* 2598 80011D98 */  cfc2        $t0, $31
    /* 259C 80011D9C */  addu        $t4, $t5, $t4
    /* 25A0 80011DA0 */  bltz        $t0, .L80011D10
    /* 25A4 80011DA4 */  sll         $t3, $t3, 16
    /* 25A8 80011DA8 */  srl         $t3, $t3, 16
    /* 25AC 80011DAC */  addu        $t3, $t5, $t3
    /* 25B0 80011DB0 */  .word 0x4B400006
    /* 25B4 80011DB4 */  swc2        $12, 0x8($t8)
    /* 25B8 80011DB8 */  mfc2        $v1, $24
    /* 25BC 80011DBC */  .word 0x4A180001
    /* 25C0 80011DC0 */  lwc2        $6, 0x10($a2)
    /* 25C4 80011DC4 */  lwc2        $0, 0x0($t1)
    /* 25C8 80011DC8 */  lwc2        $1, 0x4($t1)
    /* 25CC 80011DCC */  lwc2        $2, 0x0($t2)
    /* 25D0 80011DD0 */  lwc2        $3, 0x4($t2)
    /* 25D4 80011DD4 */  lwc2        $4, 0x0($t3)
    /* 25D8 80011DD8 */  lwc2        $5, 0x4($t3)
    /* 25DC 80011DDC */  cfc2        $t0, $31
    /* 25E0 80011DE0 */  nop
    /* 25E4 80011DE4 */  bltz        $t0, .L80011D10
    /* 25E8 80011DE8 */  nop
    /* 25EC 80011DEC */  bgtz        $v1, .L80011E0C
    /* 25F0 80011DF0 */  nop
    /* 25F4 80011DF4 */  nop
    /* 25F8 80011DF8 */  .word 0x4B400006
    /* 25FC 80011DFC */  mfc2        $t0, $24
    /* 2600 80011E00 */  nop
    /* 2604 80011E04 */  bgez        $t0, .L80011D10
    /* 2608 80011E08 */  nop
  .L80011E0C:
    /* 260C 80011E0C */  nop
  .L80011E10:
    /* 2610 80011E10 */  .word 0x4B68002E
    /* 2614 80011E14 */  sll         $t1, $t8, 8
    /* 2618 80011E18 */  srl         $t1, $t1, 8
    /* 261C 80011E1C */  .word 0x4B18043F
    /* 2620 80011E20 */  swc2        $12, 0x10($t8)
    /* 2624 80011E24 */  swc2        $13, 0x18($t8)
    /* 2628 80011E28 */  swc2        $14, 0x20($t8)
    /* 262C 80011E2C */  lwc2        $0, 0x0($t4)
    /* 2630 80011E30 */  lwc2        $1, 0x4($t4)
    /* 2634 80011E34 */  mfc2        $t2, $7
    /* 2638 80011E38 */  swc2        $20, 0x4($t8)
    /* 263C 80011E3C */  swc2        $21, 0xC($t8)
    /* 2640 80011E40 */  swc2        $22, 0x14($t8)
    /* 2644 80011E44 */  .word 0x4B08041B
    /* 2648 80011E48 */  sllv        $t2, $t2, $a1
    /* 264C 80011E4C */  andi        $t2, $t2, 0x3FFF
    /* 2650 80011E50 */  srl         $t2, $t2, 4
    /* 2654 80011E54 */  sll         $t2, $t2, 2
    /* 2658 80011E58 */  addu        $t2, $t2, $t7
    /* 265C 80011E5C */  lw          $t0, 0x0($t2)
    /* 2660 80011E60 */  sw          $t1, 0x0($t2)
    /* 2664 80011E64 */  sll         $t0, $t0, 8
    /* 2668 80011E68 */  srl         $t0, $t0, 8
    /* 266C 80011E6C */  or          $t0, $v0, $t0
    /* 2670 80011E70 */  sw          $t0, 0x0($t8)
    /* 2674 80011E74 */  swc2        $22, 0x1C($t8)
    /* 2678 80011E78 */  j           .L80011D10
    /* 267C 80011E7C */  nop
  .L80011E80:
    /* 2680 80011E80 */  sw          $t8, 0x0($a0)
  .L80011E84:
    /* 2684 80011E84 */  addu        $v0, $zero, $a2
    /* 2688 80011E88 */  jr          $ra
    /* 268C 80011E8C */  nop
  .L80011E90:
    /* Reversed-facing walk retains the reuse key even after projection failure. */
    /* 2690 80011E90 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
  .L80011E94:
    /* 2694 80011E94 */  addu        $a2, $t9, $a2
  .L80011E98:
    /* 2698 80011E98 */  beq         $zero, $a3, .L8001204C
  .L80011E9C:
    /* 269C 80011E9C */  nop
    /* 26A0 80011EA0 */  addiu       $a3, $a3, -0x1
    /* 26A4 80011EA4 */  lw          $t3, 0x4($a2)
    /* 26A8 80011EA8 */  lw          $t1, 0x0($a2)
    /* 26AC 80011EAC */  srl         $t4, $t3, 16
    /* 26B0 80011EB0 */  beq         $t4, $v1, .L80011F04
    /* 26B4 80011EB4 */  addu        $t4, $t6, $t4
    /* 26B8 80011EB8 */  lwc2        $0, 0x0($t4)
    /* 26BC 80011EBC */  lwc2        $1, 0x4($t4)
    /* 26C0 80011EC0 */  sll         $t3, $t3, 16
    /* 26C4 80011EC4 */  srl         $t3, $t3, 16
    /* 26C8 80011EC8 */  rtps
    /* 26CC 80011ECC */  addu        $t3, $t6, $t3
    /* 26D0 80011ED0 */  srl         $t2, $t1, 16
    /* 26D4 80011ED4 */  addu        $t2, $t6, $t2
    /* 26D8 80011ED8 */  sll         $t1, $t1, 16
    /* 26DC 80011EDC */  srl         $t1, $t1, 16
    /* 26E0 80011EE0 */  addu        $v1, $t1, $zero
    /* 26E4 80011EE4 */  addu        $t1, $t6, $t1
    /* 26E8 80011EE8 */  cfc2        $t0, $31
    /* 26EC 80011EEC */  nop
    /* 26F0 80011EF0 */  bltz        $t0, .L80011E90
    /* 26F4 80011EF4 */  nop
    /* 26F8 80011EF8 */  swc2        $14, 0x2C($t8)
    /* 26FC 80011EFC */  j           .L80011F2C
    /* 2700 80011F00 */  nop
  .L80011F04:
    /* 2704 80011F04 */  sll         $t3, $t3, 16
  .L80011F08:
    /* 2708 80011F08 */  srl         $t3, $t3, 16
    /* 270C 80011F0C */  addu        $t3, $t6, $t3
    /* 2710 80011F10 */  srl         $t2, $t1, 16
    /* 2714 80011F14 */  addu        $t2, $t6, $t2
    /* 2718 80011F18 */  sll         $t1, $t1, 16
    /* 271C 80011F1C */  srl         $t1, $t1, 16
    /* 2720 80011F20 */  addu        $v1, $t1, $zero
    /* 2724 80011F24 */  addu        $t1, $t6, $t1
    /* 2728 80011F28 */  sw          $v0, 0x2C($t8)
  .L80011F2C:
    /* 272C 80011F2C */  lwc2        $0, 0x0($t3)
    /* 2730 80011F30 */  lwc2        $1, 0x4($t3)
    /* 2734 80011F34 */  lwc2        $2, 0x0($t2)
    /* 2738 80011F38 */  lwc2        $3, 0x4($t2)
    /* 273C 80011F3C */  lwc2        $4, 0x0($t1)
    /* 2740 80011F40 */  lwc2        $5, 0x4($t1)
    /* 2744 80011F44 */  nop
    /* 2748 80011F48 */  nop
    /* 274C 80011F4C */  rtpt
    /* 2750 80011F50 */  lw          $t1, 0x8($a2)
    /* 2754 80011F54 */  lw          $t3, 0xC($a2)
    /* 2758 80011F58 */  srl         $t2, $t1, 16
    /* 275C 80011F5C */  addu        $t2, $t5, $t2
    /* 2760 80011F60 */  sll         $t1, $t1, 16
    /* 2764 80011F64 */  srl         $t1, $t1, 16
    /* 2768 80011F68 */  addu        $t1, $t5, $t1
    /* 276C 80011F6C */  cfc2        $t0, $31
    /* 2770 80011F70 */  srl         $t4, $t3, 16
    /* 2774 80011F74 */  bltz        $t0, .L80011E90
    /* 2778 80011F78 */  nop
    /* 277C 80011F7C */  addu        $t4, $t5, $t4
    /* 2780 80011F80 */  sll         $t3, $t3, 16
    /* Same triangles as the ordinary walk, with the accepted MAC0 signs reversed. */
    /* 2784 80011F84 */  nclip
    /* 2788 80011F88 */  mfc2        $v0, $14
    /* 278C 80011F8C */  swc2        $12, 0x20($t8)
    /* 2790 80011F90 */  swc2        $13, 0x14($t8)
    /* 2794 80011F94 */  sw          $v0, 0x8($t8)
    /* 2798 80011F98 */  mfc2        $t0, $24
    /* 279C 80011F9C */  nop
    /* 27A0 80011FA0 */  bgtz        $t0, .L80011FC4
    /* 27A4 80011FA4 */  lwc2        $14, 0x2C($t8)
    /* 27A8 80011FA8 */  nop
    /* 27AC 80011FAC */  nop
    /* 27B0 80011FB0 */  nclip
    /* 27B4 80011FB4 */  mfc2        $t0, $24
    /* 27B8 80011FB8 */  nop
    /* 27BC 80011FBC */  bgez        $t0, .L80011E90
    /* 27C0 80011FC0 */  nop
  .L80011FC4:
    /* 27C4 80011FC4 */  srl         $t3, $t3, 16
  .L80011FC8:
    /* 27C8 80011FC8 */  addu        $t3, $t5, $t3
    /* 27CC 80011FCC */  avsz4
    /* 27D0 80011FD0 */  lwc2        $0, 0x0($t1)
    /* 27D4 80011FD4 */  lwc2        $1, 0x4($t1)
    /* 27D8 80011FD8 */  lwc2        $2, 0x0($t2)
    /* 27DC 80011FDC */  lwc2        $3, 0x4($t2)
    /* 27E0 80011FE0 */  lwc2        $4, 0x0($t3)
    /* 27E4 80011FE4 */  lwc2        $5, 0x4($t3)
    /* 27E8 80011FE8 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 27EC 80011FEC */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 27F0 80011FF0 */  ncct
    /* 27F4 80011FF4 */  lwc2        $0, 0x0($t4)
    /* 27F8 80011FF8 */  lwc2        $1, 0x4($t4)
    /* 27FC 80011FFC */  mfc2        $t2, $7
    /* 2800 80012000 */  swc2        $20, 0x4($t8)
    /* 2804 80012004 */  swc2        $21, 0x10($t8)
    /* 2808 80012008 */  swc2        $22, 0x1C($t8)
    /* 280C..2840 8001200C..80012040 */  TMD_DRAW_STREAM_GT4_LINK_PACKET
    /* 2844 80012044 */  j           .L80011E90
    /* 2848 80012048 */  nop
  .L8001204C:
    /* 284C 8001204C */  sw          $t8, 0x0($a0)
  .L80012050:
    /* 2850 80012050 */  addu        $v0, $zero, $a2
    /* 2854 80012054 */  jr          $ra
    /* 2858 80012058 */  nop
.purgem TMD_DRAW_STREAM_GT4_LINK_PACKET
  .L8001205C:
    /* Reversed-facing walk: same payload, slots and lighting; keep MAC0 < 0. */
    /* 285C 8001205C */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
  .L80012060:
    /* 2860 80012060 */  addu        $a2, $t9, $a2
  .L80012064:
    /* 2864 80012064 */  beq         $zero, $a3, .L80012184
  .L80012068:
    /* 2868 80012068 */  nop
    /* 286C 8001206C */  addiu       $a3, $a3, -0x1
    /* 2870 80012070 */  lw          $t1, 0x0($a2)
    /* 2874 80012074 */  lw          $t3, 0x4($a2)
    /* 2878 80012078 */  srl         $t2, $t1, 16
    /* 287C 8001207C */  addu        $t2, $t6, $t2
    /* 2880 80012080 */  sll         $t1, $t1, 16
    /* 2884 80012084 */  srl         $t1, $t1, 16
    /* 2888 80012088 */  addu        $t1, $t6, $t1
    /* 288C 8001208C */  srl         $t4, $t3, 16
    /* 2890 80012090 */  sll         $t3, $t3, 16
    /* 2894 80012094 */  srl         $t3, $t3, 16
    /* 2898 80012098 */  addu        $t3, $t6, $t3
    /* 289C 8001209C */  lwc2        $0, 0x0($t1)
    /* 28A0 800120A0 */  lwc2        $1, 0x4($t1)
    /* 28A4 800120A4 */  lwc2        $2, 0x0($t2)
    /* 28A8 800120A8 */  lwc2        $3, 0x4($t2)
    /* 28AC 800120AC */  lwc2        $4, 0x0($t3)
    /* 28B0 800120B0 */  lwc2        $5, 0x4($t3)
    /* 28B4 800120B4 */  lw          $t2, 0x8($a2)
    /* 28B8 800120B8 */  sll         $t1, $t4, 0
    /* 28BC 800120BC */  rtpt
    /* 28C0 800120C0 */  addu        $t1, $t5, $t1
    /* 28C4 800120C4 */  srl         $t3, $t2, 16
    /* 28C8 800120C8 */  addu        $t3, $t5, $t3
    /* 28CC 800120CC */  sll         $t2, $t2, 16
    /* 28D0 800120D0 */  srl         $t2, $t2, 16
    /* 28D4 800120D4 */  addu        $t2, $t5, $t2
    /* 28D8 800120D8 */  cfc2        $t0, $31
    /* 28DC 800120DC */  nop
    /* 28E0 800120E0 */  bltz        $t0, .L8001205C
    /* 28E4 800120E4 */  nop
    /* 28E8 800120E8 */  nop
    /* 28EC 800120EC */  nop
    /* 28F0 800120F0 */  nclip
    /* 28F4 800120F4 */  mfc2        $t0, $24
    /* 28F8 800120F8 */  nop
    /* 28FC 800120FC */  bgez        $t0, .L8001205C
    /* 2900 80012100 */  nop
    /* 2904 80012104 */  swc2        $12, 0x8($t8)
    /* 2908 80012108 */  swc2        $13, 0x14($t8)
    /* 290C 8001210C */  swc2        $14, 0x20($t8)
    /* 2910 80012110 */  nop
    /* 2914 80012114 */  nop
    /* 2918 80012118 */  avsz3
    /* 291C 8001211C */  lwc2        $0, 0x0($t1)
    /* 2920 80012120 */  lwc2        $1, 0x4($t1)
    /* 2924 80012124 */  lwc2        $2, 0x0($t2)
    /* 2928 80012128 */  lwc2        $3, 0x4($t2)
    /* 292C 8001212C */  lwc2        $4, 0x0($t3)
    /* 2930 80012130 */  lwc2        $5, 0x4($t3)
    /* 2934 80012134 */  mfc2        $t2, $7
    /* 2938 80012138 */  sll         $t1, $t8, 8
    /* 293C 8001213C */  srl         $t1, $t1, 8
    /* 2940 80012140 */  ncct
    /* 2944..2978 80012144..80012178 */  TMD_DRAW_STREAM_GT3_LINK_PACKET
    /* 297C 8001217C */  j           .L8001205C
    /* 2980 80012180 */  nop
  .L80012184:
    /* 2984 80012184 */  sw          $t8, 0x0($a0)
  .L80012188:
    /* 2988 80012188 */  addu        $v0, $zero, $a2
    /* 298C 8001218C */  jr          $ra
    /* 2990 80012190 */  nop
.purgem TMD_DRAW_STREAM_GT3_LINK_PACKET
  .LtmdGt3PreXformReverseAdvance:
    /* The alternate walk changes only the accepted winding sign. */
    /* 2994 80012194 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
    /* 2998 80012198 */  addu        $a2, $t9, $a2
  .LtmdGt3PreXformReverseLoop:
    /* 299C 8001219C */  beq         $zero, $a3, .LtmdGt3PreXformReverseReturn
    /* 29A0 800121A0 */  nop
    /* 29A4 800121A4 */  lwc2        $15, 0x8($t8)
    /* 29A8 800121A8 */  lwc2        $15, 0x14($t8)
    /* 29AC 800121AC */  lwc2        $15, 0x20($t8)
    /* 29B0 800121B0 */  addiu       $a3, $a3, -0x1
    /* 29B4 800121B4 */  nop
    /* 29B8 800121B8 */  nclip
    /* 29BC 800121BC */  lw          $t1, 0x0($a2)
    /* 29C0 800121C0 */  lw          $t3, 0x4($a2)
    /* 29C4 800121C4 */  mfc2        $t0, $24
    /* 29C8 800121C8 */  srl         $t2, $t1, 16
    /* Keep strict negative winding; the same cached projection failures still reject. */
    /* 29CC 800121CC */  bgez        $t0, .LtmdGt3PreXformReverseAdvance
    /* 29D0 800121D0 */  addu        $t2, $t6, $t2
    /* 29D4 800121D4 */  lw          $t2, 0x0($t2)
    /* 29D8 800121D8 */  sll         $t1, $t1, 16
    /* 29DC 800121DC */  bltz        $t2, .LtmdGt3PreXformReverseAdvance
    /* 29E0 800121E0 */  srl         $t1, $t1, 16
    /* 29E4 800121E4 */  addu        $t1, $t6, $t1
    /* 29E8 800121E8 */  lw          $t1, 0x0($t1)
    /* 29EC 800121EC */  sll         $t3, $t3, 16
    /* 29F0 800121F0 */  bltz        $t1, .LtmdGt3PreXformReverseAdvance
    /* 29F4 800121F4 */  srl         $t3, $t3, 16
    /* 29F8 800121F8 */  addu        $t3, $t6, $t3
    /* 29FC 800121FC */  lw          $t3, 0x0($t3)
    /* 2A00 80012200 */  mtc2        $t1, $17
    /* 2A04 80012204 */  bltz        $t3, .LtmdGt3PreXformReverseAdvance
    /* 2A08 80012208 */  mtc2        $t2, $18
    /* 2A0C 8001220C */  mtc2        $t3, $19
    /* 2A10 80012210 */  lui         $t0, (TMD_DRAW_STREAM_GT3_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* 2A14 80012214 */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT3_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* 2A18 80012218 */  avsz3
    /* 2A1C 8001221C */  and         $t1, $t0, $t8
    /* 2A20 80012220 */  mfc2        $t2, $7
    /* 2A24..2A4C 80012224..8001224C */  TMD_DRAW_STREAM_GT3_PRE_XFORM_LINK_PACKET
    /* 2A50 80012250 */  j           .LtmdGt3PreXformReverseAdvance
    /* 2A54 80012254 */  nop
  .LtmdGt3PreXformReverseReturn:
    /* 2A58 80012258 */  sw          $t8, 0x4($a0)
    /* 2A5C 8001225C */  addu        $v0, $zero, $a2
    /* 2A60 80012260 */  jr          $ra
    /* 2A64 80012264 */  nop
.purgem TMD_DRAW_STREAM_GT3_PRE_XFORM_LINK_PACKET
  .LtmdGt4PreXformReverseAdvance:
    /* The alternate walk reverses both strict facing signs. */
    /* 2A68 80012268 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
    /* 2A6C 8001226C */  addu        $a2, $t9, $a2
  .LtmdGt4PreXformReverseLoop:
    /* 2A70 80012270 */  beq         $zero, $a3, .LtmdGt4PreXformReverseReturn
    /* 2A74 80012274 */  nop
    /* 2A78 80012278 */  lwc2        $15, 0x8($t8)
    /* 2A7C 8001227C */  lwc2        $15, 0x14($t8)
    /* 2A80 80012280 */  lwc2        $15, 0x20($t8)
    /* 2A84 80012284 */  addiu       $a3, $a3, -0x1
    /* 2A88 80012288 */  nop
    /* 2A8C 8001228C */  nclip
    /* 2A90 80012290 */  lw          $t1, 0x0($a2)
    /* 2A94 80012294 */  lw          $t3, 0x4($a2)
    /* 2A98 80012298 */  mfc2        $t0, $24
    /* 2A9C 8001229C */  srl         $t2, $t1, 16
    /* 2AA0 800122A0 */  bltz        $t0, .LtmdGt4PreXformReverseCheckDepths
    /* 2AA4 800122A4 */  nop
    /* The fallback keeps strict positive winding of corners 1, 2, 3. */
    /* 2AA8 800122A8 */  lwc2        $15, 0x2C($t8)
    /* 2AAC 800122AC */  nop
    /* 2AB0 800122B0 */  nop
    /* 2AB4 800122B4 */  nclip
    /* 2AB8 800122B8 */  mfc2        $t0, $24
    /* 2ABC 800122BC */  nop
    /* 2AC0 800122C0 */  blez        $t0, .LtmdGt4PreXformReverseAdvance
  .LtmdGt4PreXformReverseCheckDepths:
    /* Reversed facing still rejects each cached TMD_VERTEX_DEPTH_INVALID marker. */
    /* 2AC4 800122C4 */  addu        $t2, $t6, $t2
    /* 2AC8 800122C8 */  lw          $t2, 0x0($t2)
    /* 2ACC 800122CC */  sll         $t1, $t1, 16
    /* 2AD0 800122D0 */  bltz        $t2, .LtmdGt4PreXformReverseAdvance
    /* 2AD4 800122D4 */  srl         $t1, $t1, 16
    /* 2AD8 800122D8 */  addu        $t1, $t6, $t1
    /* 2ADC 800122DC */  lw          $t1, 0x0($t1)
    /* 2AE0 800122E0 */  srl         $t4, $t3, 16
    /* 2AE4 800122E4 */  bltz        $t1, .LtmdGt4PreXformReverseAdvance
    /* 2AE8 800122E8 */  addu        $t4, $t6, $t4
    /* 2AEC 800122EC */  lw          $t4, 0x0($t4)
    /* 2AF0 800122F0 */  sll         $t3, $t3, 16
    /* 2AF4 800122F4 */  bltz        $t4, .LtmdGt4PreXformReverseAdvance
    /* 2AF8 800122F8 */  srl         $t3, $t3, 16
    /* 2AFC 800122FC */  addu        $t3, $t6, $t3
    /* 2B00 80012300 */  lw          $t3, 0x0($t3)
    /* 2B04 80012304 */  mtc2        $t1, $16
    /* 2B08 80012308 */  bltz        $t3, .LtmdGt4PreXformReverseAdvance
    /* 2B0C 8001230C */  mtc2        $t2, $17
    /* 2B10 80012310 */  mtc2        $t3, $18
    /* 2B14 80012314 */  mtc2        $t4, $19
    /* 2B18 80012318 */  lui         $t0, (TMD_DRAW_STREAM_GT4_PRE_XFORM_DMA_ADDRESS_MASK >> 16)
    /* 2B1C 8001231C */  ori         $t0, $t0, (TMD_DRAW_STREAM_GT4_PRE_XFORM_DMA_ADDRESS_MASK & 0xFFFF)
    /* 2B20 80012320 */  avsz4
    /* 2B24 80012324 */  and         $t1, $t0, $t8
    /* 2B28 80012328 */  mfc2        $t2, $7
    /* 2B2C..2B54 8001232C..80012354 */  TMD_DRAW_STREAM_GT4_PRE_XFORM_LINK_PACKET
    /* 2B58 80012358 */  j           .LtmdGt4PreXformReverseAdvance
    /* 2B5C 8001235C */  nop
  .LtmdGt4PreXformReverseReturn:
    /* 2B60 80012360 */  sw          $t8, 0x4($a0)
    /* 2B64 80012364 */  addu        $v0, $zero, $a2
    /* 2B68 80012368 */  jr          $ra
    /* 2B6C 8001236C */  nop
.purgem TMD_DRAW_STREAM_GT4_PRE_XFORM_LINK_PACKET
/* Reuse GT3 packet/depth constants; take only the command byte, not neutral RGB. */
.equ TMD_DRAW_STREAM_GT3_CORNER_COLORS_OPAQUE_CODE, (TMD_DRAW_STREAM_GT3_COLOR >> 24)
.equ TMD_DRAW_STREAM_GT3_CORNER_COLORS_SEMI_TRANS_CODE, (TMD_DRAW_STREAM_GT3_SEMI_TRANS_COLOR >> 24)
/* GPU linked-list pointers contain the low 24 bits of a packet address. */
.equ TMD_DRAW_STREAM_GT3_CORNER_COLORS_DMA_ADDRESS_BITS, 24

/*
 * Prepend the completed triangle to its depth bucket and set its DMA tag.
 * Inputs: t8 packet, t1 its 24-bit DMA address, t7 displaced OT base,
 * t2 AVSZ3 depth, a1 depth shift, v0 nine-word length in bits 24..31.
 * Clobbers t0 and t2; writes the OT head and packet tag. Fixed registers and
 * constants belong to this handler; expansion preserves the load delay and
 * emits no call. Purged after its single use so other handlers cannot use it.
 */
.macro TMD_DRAW_STREAM_GT3_CORNER_COLORS_LINK_PACKET
    sllv        $t2, $t2, $a1
    andi        $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_MASK
    srl         $t2, $t2, TMD_DRAW_STREAM_GT3_DEPTH_SHIFT
    sll         $t2, $t2, 2
    addu        $t2, $t2, $t7
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_GT3_CORNER_COLORS_DMA_ADDRESS_BITS)
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_GT3_CORNER_COLORS_DMA_ADDRESS_BITS)
    or          $t0, $v0, $t0
    sw          $t0, 0x0($t8)
.endm

  .LtmdGt3CornerColorsSemiTrans:
    /* 2B70 80012370 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT3_CORNER_COLORS_SEMI_TRANS_CODE
    /* 2B74 80012374 */  j           .LtmdGt3CornerColorsSetup
    /* 2B78 80012378 */  nop
glabel tmdDrawStreamPrimGt3CornerColors
    /* a0 workspace, a1 object flags, a2 first element word. */
    /* 2B7C 8001237C */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 2B80 80012380 */  bnez        $t0, .LtmdGt3CornerColorsSemiTrans
    /* 2B84 80012384 */  nop
    /* 2B88 80012388 */  addiu       $v1, $zero, TMD_DRAW_STREAM_GT3_CORNER_COLORS_OPAQUE_CODE
  .LtmdGt3CornerColorsSetup:
    /* Keep element count and byte stride local; only primWrite is published. */
    /* 2B8C 8001238C */  lw          $t9, 0x18($a0)
    /* 2B90 80012390 */  lw          $a3, 0x1C($a0)
    /* 2B94 80012394 */  lw          $t8, 0x0($a0)
    /* 2B98 80012398 */  lw          $t7, 0x14($a0)
    /* 2B9C 8001239C */  lw          $t6, 0x8($a0)
    /* 2BA0 800123A0 */  lw          $t5, 0xC($a0)
    /* 2BA4 800123A4 */  sll         $t9, $t9, 2
    /* 2BA8 800123A8 */  lui         $v0, (TMD_DRAW_STREAM_GT3_PACKET_WORDS << 8)
    /* 2BAC 800123AC */  lw          $a1, 0x84($a0)
    /* 2BB0 800123B0 */  j           .LtmdGt3CornerColorsLoop
    /* 2BB4 800123B4 */  nop
  .LtmdGt3CornerColorsAdvance:
    /* Rejected triangles consume the same prebuilt packet slot as accepted ones. */
    /* 2BB8 800123B8 */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT3_PACKET_BYTES
    /* 2BBC 800123BC */  addu        $a2, $t9, $a2
  .LtmdGt3CornerColorsLoop:
    /* 2BC0 800123C0 */  beq         $zero, $a3, .LtmdGt3CornerColorsDone
    /* 2BC4 800123C4 */  nop
    /* 2BC8 800123C8 */  addiu       $a3, $a3, -0x1
    /* Unpack three vertex and three normal byte offsets from the first three words. */
    /* 2BCC 800123CC */  lw          $t1, 0x0($a2)
    /* 2BD0 800123D0 */  lw          $t3, 0x4($a2)
    /* 2BD4 800123D4 */  srl         $t2, $t1, 16
    /* 2BD8 800123D8 */  addu        $t2, $t6, $t2
    /* 2BDC 800123DC */  sll         $t1, $t1, 16
    /* 2BE0 800123E0 */  srl         $t1, $t1, 16
    /* 2BE4 800123E4 */  addu        $t1, $t6, $t1
    /* 2BE8 800123E8 */  srl         $t4, $t3, 16
    /* 2BEC 800123EC */  sll         $t3, $t3, 16
    /* 2BF0 800123F0 */  srl         $t3, $t3, 16
    /* 2BF4 800123F4 */  addu        $t3, $t6, $t3
    /* 2BF8 800123F8 */  lwc2        $0, 0x0($t1)
    /* 2BFC 800123FC */  lwc2        $1, 0x4($t1)
    /* 2C00 80012400 */  lwc2        $2, 0x0($t2)
    /* 2C04 80012404 */  lwc2        $3, 0x4($t2)
    /* 2C08 80012408 */  lwc2        $4, 0x0($t3)
    /* 2C0C 8001240C */  lwc2        $5, 0x4($t3)
    /* 2C10 80012410 */  lw          $t2, 0x8($a2)
    /* 2C14 80012414 */  sll         $t1, $t4, 0
    /* Project all corners while resolving their normal addresses. */
    /* 2C18 80012418 */  rtpt
    /* 2C1C 8001241C */  addu        $t1, $t5, $t1
    /* 2C20 80012420 */  srl         $t3, $t2, 16
    /* 2C24 80012424 */  addu        $t3, $t5, $t3
    /* 2C28 80012428 */  sll         $t2, $t2, 16
    /* 2C2C 8001242C */  srl         $t2, $t2, 16
    /* 2C30 80012430 */  addu        $t2, $t5, $t2
    /* 2C34 80012434 */  cfc2        $t0, $31
    /* 2C38 80012438 */  nop
    /* Reject projection errors and nonpositive winding; reverse-culling is ignored. */
    /* 2C3C 8001243C */  bltz        $t0, .LtmdGt3CornerColorsAdvance
    /* 2C40 80012440 */  nop
    /* 2C44 80012444 */  nop
    /* 2C48 80012448 */  nop
    /* 2C4C 8001244C */  nclip
    /* 2C50 80012450 */  mfc2        $t0, $24
    /* 2C54 80012454 */  nop
    /* 2C58 80012458 */  blez        $t0, .LtmdGt3CornerColorsAdvance
    /* 2C5C 8001245C */  nop
    /* 2C60 80012460 */  swc2        $12, 0x8($t8)
    /* 2C64 80012464 */  swc2        $13, 0x14($t8)
    /* 2C68 80012468 */  swc2        $14, 0x20($t8)
    /* 2C6C 8001246C */  nop
    /* 2C70 80012470 */  nop
    /* Light each corner's material colour with its own normal; texture stays prebuilt. */
    /* 2C74 80012474 */  avsz3
    /* 2C78 80012478 */  lwc2        $6, 0xC($a2)
    /* 2C7C 8001247C */  lwc2        $0, 0x0($t1)
    /* 2C80 80012480 */  lwc2        $1, 0x4($t1)
    /* 2C84 80012484 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT3_CORNER_COLORS_DMA_ADDRESS_BITS)
    /* 2C88 80012488 */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT3_CORNER_COLORS_DMA_ADDRESS_BITS)
    /* 2C8C 8001248C */  nccs
    /* 2C90 80012490 */  swc2        $22, 0x4($t8)
    /* 2C94 80012494 */  lwc2        $6, 0x10($a2)
    /* 2C98 80012498 */  lwc2        $0, 0x0($t2)
    /* 2C9C 8001249C */  lwc2        $1, 0x4($t2)
    /* 2CA0 800124A0 */  nop
    /* 2CA4 800124A4 */  sb          $v1, 0x7($t8)
    /* 2CA8 800124A8 */  nccs
    /* 2CAC 800124AC */  swc2        $22, 0x10($t8)
    /* 2CB0 800124B0 */  lwc2        $6, 0x14($a2)
    /* 2CB4 800124B4 */  lwc2        $0, 0x0($t3)
    /* 2CB8 800124B8 */  lwc2        $1, 0x4($t3)
    /* 2CBC 800124BC */  mfc2        $t2, $7
    /* 2CC0 800124C0 */  nop
    /* 2CC4 800124C4 */  nop
    /* 2CC8 800124C8 */  nccs
    /* 2CCC 800124CC */  swc2        $22, 0x1C($t8)
    /* 2CD0..2CF8 800124D0..800124F8 */  TMD_DRAW_STREAM_GT3_CORNER_COLORS_LINK_PACKET
    /* 2CFC 800124FC */  j           .LtmdGt3CornerColorsAdvance
    /* 2D00 80012500 */  nop
  .LtmdGt3CornerColorsDone:
    /* 2D04 80012504 */  sw          $t8, 0x0($a0)
    /* 2D08 80012508 */  addu        $v0, $zero, $a2
    /* 2D0C 8001250C */  jr          $ra
    /* 2D10 80012510 */  nop
.purgem TMD_DRAW_STREAM_GT3_CORNER_COLORS_LINK_PACKET

/* Reuse GT4 packet/depth/reuse constants; take only the GPU command byte. */
.equ TMD_DRAW_STREAM_GT4_CORNER_COLORS_OPAQUE_CODE, (TMD_DRAW_STREAM_GT4_COLOR >> 24)
.equ TMD_DRAW_STREAM_GT4_CORNER_COLORS_SEMI_TRANS_CODE, (TMD_DRAW_STREAM_GT4_SEMI_TRANS_COLOR >> 24)

/*
 * Light corner 3 and prepend the completed quad to its wrapped OT bucket.
 * Inputs: a0 workspace, t8 packet, t1 its 24-bit DMA address, t2 shifted and
 * masked AVSZ4 depth, t7 GPU command byte; GTE V0/RGB hold normal/material 3.
 * Clobbers t0..3 and NCCS GTE results; writes the OT head, twelve-word DMA
 * tag, corner-3 colour and command byte. v0/v1 retain the vertex-0 XY/reuse key.
 * Fixed registers and GT4 constants belong to this handler. Keep NCCS before
 * the link arithmetic, which supplies its latency, and the code store after
 * the colour stores. Expansion emits no call and is purged after its one use.
 */
.macro TMD_DRAW_STREAM_GT4_CORNER_COLORS_LINK_PACKET
    srl         $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_SHIFT
    lw          $t3, 0x14($a0)
    nccs
    sll         $t2, $t2, 2
    addu        $t2, $t3, $t2
    lw          $t0, 0x0($t2)
    sw          $t1, 0x0($t2)
    sll         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    lui         $t1, (TMD_DRAW_STREAM_GT4_PACKET_WORDS << (TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS - 16))
    srl         $t0, $t0, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    or          $t0, $t1, $t0
    sw          $t0, 0x0($t8)
    swc2        $22, 0x28($t8)
    sb          $t7, 0x7($t8)
.endm

  .LtmdGt4CornerColorsSemiTrans:
    /* 2D14 80012514 */  addiu       $t7, $zero, TMD_DRAW_STREAM_GT4_CORNER_COLORS_SEMI_TRANS_CODE
    /* 2D18 80012518 */  j           .LtmdGt4CornerColorsSetup
    /* 2D1C 8001251C */  nop
glabel tmdDrawStreamPrimGt4CornerColors
    /* a0 workspace, a1 object flags, a2 first element word. */
    /* 2D20 80012520 */  andi        $t0, $a1, TMD_OBJECT_SEMI_TRANS
    /* 2D24 80012524 */  bnez        $t0, .LtmdGt4CornerColorsSemiTrans
    /* 2D28 80012528 */  nop
    /* 2D2C 8001252C */  addiu       $t7, $zero, TMD_DRAW_STREAM_GT4_CORNER_COLORS_OPAQUE_CODE
  .LtmdGt4CornerColorsSetup:
    /* t9 byte stride, a3 remaining elements, t8 packet, t6/t5 geometry. */
    /* 2D30 80012530 */  lw          $t9, 0x18($a0)
    /* 2D34 80012534 */  lw          $a3, 0x1C($a0)
    /* 2D38 80012538 */  lw          $t8, 0x0($a0)
    /* 2D3C 8001253C */  lw          $t6, 0x8($a0)
    /* 2D40 80012540 */  lw          $t5, 0xC($a0)
    /* 2D44 80012544 */  sll         $t9, $t9, 2
    /* 2D48 80012548 */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
    /* 2D4C 8001254C */  lw          $a1, 0x84($a0)
    /* 2D50 80012550 */  j           .LtmdGt4CornerColorsLoop
    /* 2D54 80012554 */  nop
  .LtmdGt4CornerColorsResetReuse:
    /* A failed projection invalidates the previous vertex-0 reuse key. */
    /* 2D58 80012558 */  lui         $v1, (TMD_DRAW_STREAM_GT4_NO_PREVIOUS_VERTEX >> 16)
  .LtmdGt4CornerColorsAdvance:
    /* Rejected quads consume the same prebuilt packet slot as accepted ones. */
    /* 2D5C 8001255C */  addiu       $t8, $t8, TMD_DRAW_STREAM_GT4_PACKET_BYTES
    /* 2D60 80012560 */  addu        $a2, $t9, $a2
  .LtmdGt4CornerColorsLoop:
    /* 2D64 80012564 */  beq         $zero, $a3, .LtmdGt4CornerColorsDone
    /* 2D68 80012568 */  nop
    /* 2D6C 8001256C */  addiu       $a3, $a3, -0x1
    /* 2D70 80012570 */  lw          $t3, 0x4($a2)
    /* 2D74 80012574 */  lw          $t1, 0x0($a2)
    /* 2D78 80012578 */  srl         $t4, $t3, 16
    /* Reuse vertex 3 when it is the preceding element's vertex 0 (v1 key, v0 XY). */
    /* 2D7C 8001257C */  beq         $t4, $v1, .LtmdGt4CornerColorsReuseVertex3
    /* 2D80 80012580 */  addu        $t4, $t6, $t4
    /* 2D84 80012584 */  lwc2        $0, 0x0($t4)
    /* 2D88 80012588 */  lwc2        $1, 0x4($t4)
    /* 2D8C 8001258C */  sll         $t3, $t3, 16
    /* 2D90 80012590 */  srl         $t3, $t3, 16
    /* 2D94 80012594 */  rtps
    /* 2D98 80012598 */  addu        $t3, $t6, $t3
    /* 2D9C 8001259C */  srl         $t2, $t1, 16
    /* 2DA0 800125A0 */  addu        $t2, $t6, $t2
    /* 2DA4 800125A4 */  sll         $t1, $t1, 16
    /* 2DA8 800125A8 */  srl         $t1, $t1, 16
    /* 2DAC 800125AC */  addu        $v1, $t1, $zero
    /* 2DB0 800125B0 */  addu        $t1, $t6, $t1
    /* 2DB4 800125B4 */  cfc2        $t0, $31
    /* 2DB8 800125B8 */  nop
    /* 2DBC 800125BC */  bltz        $t0, .LtmdGt4CornerColorsResetReuse
    /* 2DC0 800125C0 */  nop
    /* 2DC4 800125C4 */  swc2        $14, 0x2C($t8)
    /* 2DC8 800125C8 */  j           .LtmdGt4CornerColorsProjectFirstThree
    /* 2DCC 800125CC */  nop
  .LtmdGt4CornerColorsReuseVertex3:
    /* Keep the preceding depth in the GTE FIFO; copy its cached XY into this packet. */
    /* 2DD0 800125D0 */  sll         $t3, $t3, 16
    /* 2DD4 800125D4 */  srl         $t3, $t3, 16
    /* 2DD8 800125D8 */  addu        $t3, $t6, $t3
    /* 2DDC 800125DC */  srl         $t2, $t1, 16
    /* 2DE0 800125E0 */  addu        $t2, $t6, $t2
    /* 2DE4 800125E4 */  sll         $t1, $t1, 16
    /* 2DE8 800125E8 */  srl         $t1, $t1, 16
    /* 2DEC 800125EC */  addu        $v1, $t1, $zero
    /* 2DF0 800125F0 */  addu        $t1, $t6, $t1
    /* 2DF4 800125F4 */  sw          $v0, 0x2C($t8)
  .LtmdGt4CornerColorsProjectFirstThree:
    /* Project vertices in 2,1,0 order while resolving four normal byte offsets. */
    /* 2DF8 800125F8 */  lwc2        $0, 0x0($t3)
    /* 2DFC 800125FC */  lwc2        $1, 0x4($t3)
    /* 2E00 80012600 */  lwc2        $2, 0x0($t2)
    /* 2E04 80012604 */  lwc2        $3, 0x4($t2)
    /* 2E08 80012608 */  lwc2        $4, 0x0($t1)
    /* 2E0C 8001260C */  lwc2        $5, 0x4($t1)
    /* 2E10 80012610 */  nop
    /* 2E14 80012614 */  nop
    /* 2E18 80012618 */  rtpt
    /* 2E1C 8001261C */  lw          $t1, 0x8($a2)
    /* 2E20 80012620 */  lw          $t3, 0xC($a2)
    /* 2E24 80012624 */  srl         $t2, $t1, 16
    /* 2E28 80012628 */  addu        $t2, $t5, $t2
    /* 2E2C 8001262C */  sll         $t1, $t1, 16
    /* 2E30 80012630 */  srl         $t1, $t1, 16
    /* 2E34 80012634 */  addu        $t1, $t5, $t1
    /* 2E38 80012638 */  cfc2        $t0, $31
    /* 2E3C 8001263C */  srl         $t4, $t3, 16
    /* 2E40 80012640 */  bltz        $t0, .LtmdGt4CornerColorsResetReuse
    /* 2E44 80012644 */  nop
    /* 2E48 80012648 */  addu        $t4, $t5, $t4
    /* 2E4C 8001264C */  sll         $t3, $t3, 16
    /* Keep NCLIP(2,1,0) < 0 or NCLIP(2,1,3) > 0; reverse-culling is ignored. */
    /* 2E50 80012650 */  nclip
    /* 2E54 80012654 */  mfc2        $v0, $14
    /* 2E58 80012658 */  swc2        $12, 0x20($t8)
    /* 2E5C 8001265C */  swc2        $13, 0x14($t8)
    /* 2E60 80012660 */  sw          $v0, 0x8($t8)
    /* 2E64 80012664 */  mfc2        $t0, $24
    /* 2E68 80012668 */  nop
    /* 2E6C 8001266C */  bltz        $t0, .LtmdGt4CornerColorsLight
    /* 2E70 80012670 */  lwc2        $14, 0x2C($t8)
    /* 2E74 80012674 */  nop
    /* 2E78 80012678 */  nop
    /* 2E7C 8001267C */  nclip
    /* 2E80 80012680 */  mfc2        $t0, $24
    /* 2E84 80012684 */  nop
    /* 2E88 80012688 */  blez        $t0, .LtmdGt4CornerColorsAdvance
    /* 2E8C 8001268C */  nop
  .LtmdGt4CornerColorsLight:
    /* Light each corner's material with its own normal; texture fields stay prebuilt. */
    /* 2E90 80012690 */  srl         $t3, $t3, 16
    /* 2E94 80012694 */  addu        $t3, $t5, $t3
    /* 2E98 80012698 */  avsz4
    /* 2E9C 8001269C */  lwc2        $6, 0x10($a2)
    /* 2EA0 800126A0 */  lwc2        $0, 0x0($t1)
    /* 2EA4 800126A4 */  lwc2        $1, 0x4($t1)
    /* 2EA8 800126A8 */  sll         $t1, $t8, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 2EAC 800126AC */  srl         $t1, $t1, (32 - TMD_DRAW_STREAM_GT4_DMA_ADDRESS_BITS)
    /* 2EB0 800126B0 */  nccs
    /* 2EB4 800126B4 */  swc2        $22, 0x4($t8)
    /* 2EB8 800126B8 */  lwc2        $6, 0x14($a2)
    /* 2EBC 800126BC */  lwc2        $0, 0x0($t2)
    /* 2EC0 800126C0 */  lwc2        $1, 0x4($t2)
    /* 2EC4 800126C4 */  mfc2        $t2, $7
    /* 2EC8 800126C8 */  nop
    /* 2ECC 800126CC */  nop
    /* 2ED0 800126D0 */  nccs
    /* 2ED4 800126D4 */  swc2        $22, 0x10($t8)
    /* 2ED8 800126D8 */  lwc2        $6, 0x18($a2)
    /* 2EDC 800126DC */  lwc2        $0, 0x0($t3)
    /* 2EE0 800126E0 */  lwc2        $1, 0x4($t3)
    /* 2EE4 800126E4 */  sllv        $t2, $t2, $a1
    /* 2EE8 800126E8 */  andi        $t2, $t2, TMD_DRAW_STREAM_GT4_DEPTH_MASK
    /* 2EEC 800126EC */  nccs
    /* 2EF0 800126F0 */  swc2        $22, 0x1C($t8)
    /* 2EF4 800126F4 */  lwc2        $6, 0x1C($a2)
    /* 2EF8 800126F8 */  lwc2        $0, 0x0($t4)
    /* 2EFC 800126FC */  lwc2        $1, 0x4($t4)
    /* 2F00..2F34 80012700..80012734 */  TMD_DRAW_STREAM_GT4_CORNER_COLORS_LINK_PACKET
    /* 2F38 80012738 */  j           .LtmdGt4CornerColorsAdvance
    /* 2F3C 8001273C */  nop
  .LtmdGt4CornerColorsDone:
    /* 2F40 80012740 */  sw          $t8, 0x0($a0)
    /* 2F44 80012744 */  addu        $v0, $zero, $a2
    /* 2F48 80012748 */  jr          $ra
    /* 2F4C 8001274C */  nop
endlabel tmdDrawStreamPrimGt4CornerColors
.purgem TMD_DRAW_STREAM_GT4_CORNER_COLORS_LINK_PACKET
