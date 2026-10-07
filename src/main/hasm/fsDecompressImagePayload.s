.include "macro.inc"

.set noat
.set noreorder

/*
 * fsDecompressImagePayload  (VRAM 0x80010398 / ROM 0xB98)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 *
 * Contract documented at its declaration in src/main/fs.h.
 *
 * Why this stays handwritten assembly
 *     - signed MIPS `sub` / `addi` opcodes rather than GCC 2.8.1's unsigned forms
 *     - early-image placement (linker_section_order: .rodata)
 *     - tight register machine / delay-slot packing
 *   Do not convert to type: c or INCLUDE_ASM from a regular TU.
 */

.equ FILE_SYSTEM_IMAGE_LZSS_WINDOW_BYTES, 256
.equ FILE_SYSTEM_IMAGE_LZSS_WINDOW_MASK, FILE_SYSTEM_IMAGE_LZSS_WINDOW_BYTES - 1
.equ FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS, 8
.equ FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK, (1 << FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS) - 1
.equ FILE_SYSTEM_IMAGE_LZSS_LENGTH_BITS, 4
.equ FILE_SYSTEM_IMAGE_LZSS_MIN_MATCH_BYTES, 2
.equ FILE_SYSTEM_IMAGE_LZSS_INITIAL_WRITE_INDEX, 1
/* Status values also declared beside the C interface in src/main/fs.h. */
.equ FILE_SYSTEM_IMAGE_DECODE_COMPLETE, 1
.equ FILE_SYSTEM_IMAGE_DECODE_SCRATCH_BUSY, 0xFFFF
/* Low halfword of the initialized scratch-stack cursor is its byte offset. */
.equ FILE_SYSTEM_IMAGE_LZSS_SCRATCH_BASE, 0x1F800000
.equ FILE_SYSTEM_IMAGE_LZSS_SCRATCH_CURSOR, 0x1F8003FC

/*
 * Read an eight-bit value across the current bit-byte boundary.
 * Requires $t6 = FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS; result is $t5 or $t7.
 * Captures and updates the
 * input cursor $t0, unread-bit mask $t2, byte $t3 and bit count $a2; clobbers
 * $a1 and $t6. Eagerly fetches the next byte even when the value is aligned.
 * Expands inline, with unique labels and the original load/delay-slot order.
 */
.macro FILE_SYSTEM_IMAGE_LZSS_READ_BYTE result
    bnez       $t2, .LfsImageReadByteCurrent\@
     nop
    addiu      $t2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    addiu      $a2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    lbu        $t3, 0x0($t0)
    addiu      $t0, $t0, 0x1
  .LfsImageReadByteCurrent\@:
    and        $a1, $t2, $t3
    addu       \result, $a1, $zero
    sub        $t6, $t6, $a2
    addiu      $t2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    addiu      $a2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    lbu        $t3, 0x0($t0)
    addiu      $t0, $t0, 0x1
    beqz       $t6, .LfsImageReadByteDone\@
     nop
    srlv       $t2, $t2, $t6
    sub        $a2, $a2, $t6
    xori       $a1, $t2, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    and        $a1, $a1, $t3
    sllv       \result, \result, $t6
    srlv       $a1, $a1, $a2
    or         \result, \result, $a1
  .LfsImageReadByteDone\@:
.endm

.section .text, "ax"

glabel fsDecompressImagePayload
    /* Keep the history ring below the active downward-growing scratch stack. */
    /* B98 80010398 801F073C */  lui        $a3, (FILE_SYSTEM_IMAGE_LZSS_SCRATCH_BASE >> 16)
    /* B9C 8001039C 21280000 */  addu       $a1, $zero, $zero
    /* BA0 800103A0 FC03E584 */  lh         $a1, (FILE_SYSTEM_IMAGE_LZSS_SCRATCH_CURSOR & 0xFFFF)($a3)
    /* BA4 800103A4 00010424 */  addiu      $a0, $zero, FILE_SYSTEM_IMAGE_LZSS_WINDOW_BYTES
    /* BA8 800103A8 2230A400 */  sub        $a2, $a1, $a0 /* handwritten instruction */
    /* BAC 800103AC 0500C01C */  bgtz       $a2, .L800103C4
    /* BB0 800103B0 FFFF0434 */   ori       $a0, $zero, FILE_SYSTEM_IMAGE_DECODE_SCRATCH_BUSY
    /* BB4 800103B4 0780013C */  lui        $at, %hi(D5B498_8006D748)
    /* BB8 800103B8 48D724A4 */  sh         $a0, %lo(D5B498_8006D748)($at)
    /* BBC 800103BC 0800E003 */  jr         $ra
    /* BC0 800103C0 00000000 */   nop
  .L800103C4:
    /* Start a new bit cursor over the existing, uncleared history ring. */
    /* BC4 800103C4 0780083C */  lui        $t0, %hi(Fs_ChunkReadPtr)
    /* BC8 800103C8 2CC2088D */  lw         $t0, %lo(Fs_ChunkReadPtr)($t0)
    /* BCC 800103CC 0780093C */  lui        $t1, %hi(Fs_ChunkWritePtr)
    /* BD0 800103D0 D8D4298D */  lw         $t1, %lo(Fs_ChunkWritePtr)($t1)
    /* BD4 800103D4 21500000 */  addu       $t2, $zero, $zero
    /* BD8 800103D8 21580000 */  addu       $t3, $zero, $zero
    /* BDC 800103DC 21300000 */  addu       $a2, $zero, $zero
    /* BE0 800103E0 01000C24 */  addiu      $t4, $zero, FILE_SYSTEM_IMAGE_LZSS_INITIAL_WRITE_INDEX
  .L800103E4:
    /* Consume an MSB-first flag: 1 is a literal, 0 is a match or end token. */
    /* BE4 800103E4 05004015 */  bnez       $t2, .L800103FC
    /* BE8 800103E8 00000000 */   nop
    /* BEC 800103EC FF000A24 */  addiu      $t2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    /* BF0 800103F0 08000624 */  addiu      $a2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    /* BF4 800103F4 00000B91 */  lbu        $t3, 0x0($t0)
    /* BF8 800103F8 01000825 */  addiu      $t0, $t0, 0x1
  .L800103FC:
    /* BFC 800103FC 24684B01 */  and        $t5, $t2, $t3
    /* C00 80010400 42500A00 */  srl        $t2, $t2, 1
    /* C04 80010404 FFFFC620 */  addi       $a2, $a2, -0x1 /* handwritten instruction */
    /* C08 80010408 FF004439 */  xori       $a0, $t2, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    /* C0C 8001040C 2468A401 */  and        $t5, $t5, $a0
    /* C10 80010410 1F00A019 */  blez       $t5, .L80010490
    /* C14 80010414 08000E24 */   addiu     $t6, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    /* C18 80010418 00000D24 */  addiu      $t5, $zero, 0x0
    /* C1C..C70: extract a literal byte, then append it to history and output. */
    FILE_SYSTEM_IMAGE_LZSS_READ_BYTE $t5
    /* C74 80010474 2110EC00 */  addu       $v0, $a3, $t4
    /* C78 80010478 00004DA0 */  sb         $t5, (FILE_SYSTEM_IMAGE_LZSS_SCRATCH_BASE & 0xFFFF)($v0)
    /* C7C 8001047C 00002DA1 */  sb         $t5, 0x0($t1)
    /* C80 80010480 01008C25 */  addiu      $t4, $t4, 0x1
    /* C84 80010484 FF008C31 */  andi       $t4, $t4, FILE_SYSTEM_IMAGE_LZSS_WINDOW_MASK
    /* C88 80010488 01002925 */  addiu      $t1, $t1, 0x1
    /* C8C 8001048C F9400008 */  j          .L800103E4
  .L80010490:
    /* C90 80010490 08000E24 */   addiu     $t6, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    /* C94 80010494 00000F24 */  addiu      $t7, $zero, 0x0
    /* C98..CEC: extract an absolute ring index; zero terminates the payload. */
    FILE_SYSTEM_IMAGE_LZSS_READ_BYTE $t7
    /* CF0 800104F0 2900E011 */  beqz       $t7, .L80010598
    /* CF4 800104F4 04000E24 */   addiu     $t6, $zero, FILE_SYSTEM_IMAGE_LZSS_LENGTH_BITS
    /* CF8 800104F8 00000D24 */  addiu      $t5, $zero, 0x0
    /* CFC 800104FC 05004015 */  bnez       $t2, .L80010514
    /* D00 80010500 00000000 */   nop
    /* D04 80010504 FF000A24 */  addiu      $t2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    /* D08 80010508 08000624 */  addiu      $a2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    /* D0C 8001050C 00000B91 */  lbu        $t3, 0x0($t0)
    /* D10 80010510 01000825 */  addiu      $t0, $t0, 0x1
  .L80010514:
    /* D14 80010514 2220C601 */  sub        $a0, $t6, $a2 /* handwritten instruction */
    /* D18 80010518 07008018 */  blez       $a0, .L80010538
    /* D1C 8001051C 00000000 */   nop
    /* D20 80010520 21708000 */  addu       $t6, $a0, $zero
    /* D24 80010524 24684B01 */  and        $t5, $t2, $t3
    /* D28 80010528 FF000A24 */  addiu      $t2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    /* D2C 8001052C 08000624 */  addiu      $a2, $zero, FILE_SYSTEM_IMAGE_LZSS_BYTE_BITS
    /* D30 80010530 00000B91 */  lbu        $t3, 0x0($t0)
    /* D34 80010534 01000825 */  addiu      $t0, $t0, 0x1
  .L80010538:
    /* D38 80010538 24284B01 */  and        $a1, $t2, $t3
    /* D3C 8001053C 0650CA01 */  srlv       $t2, $t2, $t6
    /* D40 80010540 2230CE00 */  sub        $a2, $a2, $t6 /* handwritten instruction */
    /* D44 80010544 FF004439 */  xori       $a0, $t2, FILE_SYSTEM_IMAGE_LZSS_BYTE_MASK
    /* D48 80010548 2428A400 */  and        $a1, $a1, $a0
    /* D4C 8001054C 0468CD01 */  sllv       $t5, $t5, $t6
    /* D50 80010550 0628C500 */  srlv       $a1, $a1, $a2
    /* D54 80010554 2568A501 */  or         $t5, $t5, $a1
    /* The inclusive countdown copies length nibble + 2 bytes, including overlap. */
    /* D58 80010558 0100A325 */  addiu      $v1, $t5, FILE_SYSTEM_IMAGE_LZSS_MIN_MATCH_BYTES - 1
  .L8001055C:
    /* D5C 8001055C FF00E431 */  andi       $a0, $t7, FILE_SYSTEM_IMAGE_LZSS_WINDOW_MASK
    /* D60 80010560 2110E400 */  addu       $v0, $a3, $a0
    /* D64 80010564 00004D90 */  lbu        $t5, 0x0($v0)
    /* D68 80010568 2110EC00 */  addu       $v0, $a3, $t4
    /* D6C 8001056C 00004DA0 */  sb         $t5, 0x0($v0)
    /* D70 80010570 00002DA1 */  sb         $t5, 0x0($t1)
    /* D74 80010574 01008C25 */  addiu      $t4, $t4, 0x1
    /* D78 80010578 FF008C31 */  andi       $t4, $t4, FILE_SYSTEM_IMAGE_LZSS_WINDOW_MASK
    /* D7C 8001057C 01002925 */  addiu      $t1, $t1, 0x1
    /* D80 80010580 0100EF25 */  addiu      $t7, $t7, 0x1
    /* D84 80010584 FFFF6320 */  addi       $v1, $v1, -0x1 /* handwritten instruction */
    /* D88 80010588 F4FF6104 */  bgez       $v1, .L8001055C
    /* D8C 8001058C 00000000 */   nop
    /* D90 80010590 F9400008 */  j          .L800103E4
    /* D94 80010594 00000000 */   nop
  .L80010598:
    /* Completion is a status store; cursors remain at their caller-supplied bases. */
    /* D98 80010598 01000424 */  addiu      $a0, $zero, FILE_SYSTEM_IMAGE_DECODE_COMPLETE
    /* D9C 8001059C 0780013C */  lui        $at, %hi(D5B498_8006D748)
    /* DA0 800105A0 48D724A4 */  sh         $a0, %lo(D5B498_8006D748)($at)
    /* DA4 800105A4 0800E003 */  jr         $ra
    /* DA8 800105A8 00000000 */   nop
endlabel fsDecompressImagePayload
