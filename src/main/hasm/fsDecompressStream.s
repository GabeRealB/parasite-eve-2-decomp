.include "macro.inc"

.set noat
.set noreorder

/*
 * fsDecompressStream (VRAM 0x80010024 / ROM 0x824).
 * Contract documented at its declaration in src/main/fs.h.
 *
 * The resume table and code assemble as one hasm unit, with .rodata before
 * .text in the early image. Signed sub/addi and fixed mid-function resume
 * addresses require preserving this instruction schedule.
 */

.equ FILE_SYSTEM_STREAM_LZSS_WINDOW_BYTES, 256
.equ FILE_SYSTEM_STREAM_LZSS_WINDOW_MASK, FILE_SYSTEM_STREAM_LZSS_WINDOW_BYTES - 1
.equ FILE_SYSTEM_STREAM_LZSS_BYTE_BITS, 8
.equ FILE_SYSTEM_STREAM_LZSS_BYTE_MASK, (1 << FILE_SYSTEM_STREAM_LZSS_BYTE_BITS) - 1
.equ FILE_SYSTEM_STREAM_LZSS_LENGTH_BITS, 4
.equ FILE_SYSTEM_STREAM_LZSS_MIN_MATCH_BYTES, 2
/* Status values also declared beside the C interface in src/main/fs.h. */
.equ FILE_SYSTEM_STREAM_DECODE_COMPLETE, 1
.equ FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY, 0xFFFF
/* The initialized scratch-stack pointer's low halfword is its byte offset. */
.equ FILE_SYSTEM_STREAM_LZSS_SCRATCH_BASE, 0x1F800000
.equ FILE_SYSTEM_STREAM_LZSS_SCRATCH_CURSOR, 0x1F8003FC

/*
 * Fetch a bitstream byte and suspend if the advanced input cursor equals its end.
 * resumeOffset selects a table entry in bytes (0, 4, ... 24); resumeLabel is
 * that entry's label immediately after this expansion. Requires a readable
 * byte at $t0 and $t8 = exclusive input end. Updates $t0, $t2, $t3 and $a2;
 * clobbers $a0 and $at. At the boundary, saves the continuation address and
 * jumps to the shared state-save exit without consuming the loaded byte.
 * The caller supplies the jump delay slot at resumeLabel, which also runs on
 * suspension, and must preserve its original position and side effects.
 */
.macro FILE_SYSTEM_STREAM_LZSS_REFILL resumeOffset, resumeLabel
    addiu      $t2, $zero, FILE_SYSTEM_STREAM_LZSS_BYTE_MASK
    addiu      $a2, $zero, FILE_SYSTEM_STREAM_LZSS_BYTE_BITS
    lbu        $t3, 0x0($t0)
    addiu      $t0, $t0, 0x1
    subu       $a0, $t8, $t0
    bnez       $a0, \resumeLabel
     lui       $a0, %hi(jtbl_Fs_DecompressChunk)
    lw         $a0, %lo(jtbl_Fs_DecompressChunk + \resumeOffset)($a0)
    lui        $at, %hi(D5B498_8006D850)
    sw         $a0, %lo(D5B498_8006D850)($at)
    j          .LfsStreamSaveState
.endm

.section .rodata, "a"

/* Resume jump table (ROM 0x808 / VRAM 0x80010008) */

dlabel jtbl_Fs_DecompressChunk
    /* 808 80010008 */ .word .LfsStreamResumeTokenFlag /* +0x00 flag-bit refill */
    /* 80C 8001000C */ .word .LfsStreamResumeLiteral /* +0x04 literal refill */
    /* 810 80010010 */ .word .LfsStreamFinishLiteral /* +0x08 literal 2nd byte */
    /* 814 80010014 */ .word .LfsStreamResumeMatchIndex /* +0x0C match-offset refill */
    /* 818 80010018 */ .word .LfsStreamFinishMatchIndex /* +0x10 match-offset 2nd byte */
    /* 81C 8001001C */ .word .LfsStreamResumeMatchLength /* +0x14 match-length refill */
    /* 820 80010020 */ .word .LfsStreamFinishMatchLength /* +0x18 match-length 2nd byte */
enddlabel jtbl_Fs_DecompressChunk

.section .text, "ax"

/* Keep the history ring below the live scratch stack. */

glabel fsDecompressStream
    /* 824 80010024 801F073C */  lui        $a3, (FILE_SYSTEM_STREAM_LZSS_SCRATCH_BASE >> 16)
    /* 828 80010028 21280000 */  addu       $a1, $zero, $zero
    /* 82C 8001002C FC03E584 */  lh         $a1, (FILE_SYSTEM_STREAM_LZSS_SCRATCH_CURSOR & 0xFFFF)($a3)
    /* 830 80010030 00010424 */  addiu      $a0, $zero, FILE_SYSTEM_STREAM_LZSS_WINDOW_BYTES
    /* 834 80010034 2230A400 */  sub        $a2, $a1, $a0 /* handwritten instruction */
    /* 838 80010038 0500C01C */  bgtz       $a2, .LfsStreamRestoreState
    /* 83C 8001003C FFFF0434 */   ori       $a0, $zero, FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY
    /* 840 80010040 0780013C */  lui        $at, %hi(D5B498_8006D748)
    /* 844 80010044 48D724A4 */  sh         $a0, %lo(D5B498_8006D748)($at)
    /* 848 80010048 0800E003 */  jr         $ra
    /* 84C 8001004C 00000000 */   nop
  .LfsStreamRestoreState:
    /* Restore the bit reservoir and any token split across input intervals. */
    /* 850 80010050 0780083C */  lui        $t0, %hi(Fs_ChunkReadPtr)
    /* 854 80010054 2CC2088D */  lw         $t0, %lo(Fs_ChunkReadPtr)($t0)
    /* 858 80010058 0780183C */  lui        $t8, %hi(D_8006C4D4)
    /* 85C 8001005C D4C4188F */  lw         $t8, %lo(D_8006C4D4)($t8)
    /* 860 80010060 0780093C */  lui        $t1, %hi(Fs_ChunkWritePtr)
    /* 864 80010064 D8D4298D */  lw         $t1, %lo(Fs_ChunkWritePtr)($t1)
    /* 868 80010068 07800A3C */  lui        $t2, %hi(D5B498_8006EBB0)
    /* 86C 8001006C B0EB4A95 */  lhu        $t2, %lo(D5B498_8006EBB0)($t2)
    /* 870 80010070 07800B3C */  lui        $t3, %hi(D5B498_8006EA1A)
    /* 874 80010074 1AEA6B95 */  lhu        $t3, %lo(D5B498_8006EA1A)($t3)
    /* 878 80010078 07800C3C */  lui        $t4, %hi(D5B498_8006D858)
    /* 87C 8001007C 58D88C95 */  lhu        $t4, %lo(D5B498_8006D858)($t4)
    /* 880 80010080 0780063C */  lui        $a2, %hi(D5B498_8006D85A)
    /* 884 80010084 5AD8C694 */  lhu        $a2, %lo(D5B498_8006D85A)($a2)
    /* 888 80010088 07800D3C */  lui        $t5, %hi(D_8006EA08)
    /* 88C 8001008C 08EAAD85 */  lh         $t5, %lo(D_8006EA08)($t5)
    /* 890 80010090 07800E3C */  lui        $t6, %hi(D_8006EA0A)
    /* 894 80010094 0AEACE85 */  lh         $t6, %lo(D_8006EA0A)($t6)
    /* 898 80010098 07800F3C */  lui        $t7, %hi(D_8006EA0C)
    /* 89C 8001009C 0CEAEF8D */  lw         $t7, %lo(D_8006EA0C)($t7)
    /* 8A0 800100A0 0780043C */  lui        $a0, %hi(D5B498_8006D850)
    /* 8A4 800100A4 50D8848C */  lw         $a0, %lo(D5B498_8006D850)($a0)
    /* 8A8 800100A8 0780013C */  lui        $at, %hi(D5B498_8006D850)
    /* 8AC 800100AC 50D820AC */  sw         $zero, %lo(D5B498_8006D850)($at)
    /* 8B0 800100B0 03008010 */  beqz       $a0, .LfsStreamReadTokenFlag
    /* 8B4 800100B4 00000000 */   nop
    /* 8B8 800100B8 08008000 */  jr         $a0
    /* 8BC 800100BC 00000000 */   nop
  .LfsStreamReadTokenFlag:
    /* A high flag bit selects a literal; a low bit selects a history match. */
    /* 8C0 800100C0 0C004015 */  bnez       $t2, .LfsStreamResumeTokenFlag
    /* 8C4 800100C4 00000000 */   nop
    FILE_SYSTEM_STREAM_LZSS_REFILL 0, .LfsStreamResumeTokenFlag
  .LfsStreamResumeTokenFlag:
    /* 8F4 800100F4 24684B01 */   and       $t5, $t2, $t3
    /* 8F8 800100F8 42500A00 */  srl        $t2, $t2, 1
    /* 8FC 800100FC FFFFC620 */  addi       $a2, $a2, -0x1 /* handwritten instruction */
    /* 900 80010100 FF004439 */  xori       $a0, $t2, FILE_SYSTEM_STREAM_LZSS_BYTE_MASK
    /* 904 80010104 2468A401 */  and        $t5, $t5, $a0
    /* 908 80010108 2E00A019 */  blez       $t5, .LfsStreamReadMatchIndex
    /* 90C 8001010C 08000E24 */   addiu     $t6, $zero, FILE_SYSTEM_STREAM_LZSS_BYTE_BITS
    /* 910 80010110 00000D24 */  addiu      $t5, $zero, 0x0
    /* 914 80010114 0C004015 */  bnez       $t2, .LfsStreamResumeLiteral
    /* 918 80010118 00000000 */   nop
    FILE_SYSTEM_STREAM_LZSS_REFILL 4, .LfsStreamResumeLiteral
  .LfsStreamResumeLiteral:
    /* 948 80010148 24284B01 */   and       $a1, $t2, $t3
    /* 94C 8001014C 2168A000 */  addu       $t5, $a1, $zero
    /* 950 80010150 2270C601 */  sub        $t6, $t6, $a2 /* handwritten instruction */
    FILE_SYSTEM_STREAM_LZSS_REFILL 8, .LfsStreamFinishLiteral
  .LfsStreamFinishLiteral:
    /* 980 80010180 00000000 */   nop
    /* 984 80010184 0800C011 */  beqz       $t6, .LfsStreamWriteLiteral
    /* 988 80010188 00000000 */   nop
    /* 98C 8001018C 0650CA01 */  srlv       $t2, $t2, $t6
    /* 990 80010190 2230CE00 */  sub        $a2, $a2, $t6 /* handwritten instruction */
    /* 994 80010194 FF004539 */  xori       $a1, $t2, FILE_SYSTEM_STREAM_LZSS_BYTE_MASK
    /* 998 80010198 2428AB00 */  and        $a1, $a1, $t3
    /* 99C 8001019C 0468CD01 */  sllv       $t5, $t5, $t6
    /* 9A0 800101A0 0628C500 */  srlv       $a1, $a1, $a2
    /* 9A4 800101A4 2568A501 */  or         $t5, $t5, $a1
  .LfsStreamWriteLiteral:
    /* 9A8 800101A8 2110EC00 */  addu       $v0, $a3, $t4
    /* 9AC 800101AC 00004DA0 */  sb         $t5, 0x0($v0)
    /* 9B0 800101B0 00002DA1 */  sb         $t5, 0x0($t1)
    /* 9B4 800101B4 01008C25 */  addiu      $t4, $t4, 0x1
    /* 9B8 800101B8 FF008C31 */  andi       $t4, $t4, FILE_SYSTEM_STREAM_LZSS_WINDOW_MASK
    /* 9BC 800101BC 01002925 */  addiu      $t1, $t1, 0x1
    /* 9C0 800101C0 30400008 */  j          .LfsStreamReadTokenFlag
  .LfsStreamReadMatchIndex:
    /* 9C4 800101C4 08000E24 */   addiu     $t6, $zero, FILE_SYSTEM_STREAM_LZSS_BYTE_BITS
    /* 9C8 800101C8 00000F24 */  addiu      $t7, $zero, 0x0
    /* 9CC 800101CC 0C004015 */  bnez       $t2, .LfsStreamResumeMatchIndex
    /* 9D0 800101D0 00000000 */   nop
    FILE_SYSTEM_STREAM_LZSS_REFILL 12, .LfsStreamResumeMatchIndex
  .LfsStreamResumeMatchIndex:
    /* A00 80010200 24284B01 */   and       $a1, $t2, $t3
    /* A04 80010204 2178A000 */  addu       $t7, $a1, $zero
    /* A08 80010208 2270C601 */  sub        $t6, $t6, $a2 /* handwritten instruction */
    FILE_SYSTEM_STREAM_LZSS_REFILL 16, .LfsStreamFinishMatchIndex
  .LfsStreamFinishMatchIndex:
    /* A38 80010238 00000000 */   nop
    /* A3C 8001023C 0800C011 */  beqz       $t6, .LfsStreamReadMatchLength
    /* A40 80010240 00000000 */   nop
    /* A44 80010244 0650CA01 */  srlv       $t2, $t2, $t6
    /* A48 80010248 2230CE00 */  sub        $a2, $a2, $t6 /* handwritten instruction */
    /* A4C 8001024C FF004539 */  xori       $a1, $t2, FILE_SYSTEM_STREAM_LZSS_BYTE_MASK
    /* A50 80010250 2428AB00 */  and        $a1, $a1, $t3
    /* A54 80010254 0478CF01 */  sllv       $t7, $t7, $t6
    /* A58 80010258 0628C500 */  srlv       $a1, $a1, $a2
    /* A5C 8001025C 2578E501 */  or         $t7, $t7, $a1
  .LfsStreamReadMatchLength:
    /* A60 80010260 3600E011 */  beqz       $t7, .LfsStreamComplete
    /* A64 80010264 04000E24 */   addiu     $t6, $zero, FILE_SYSTEM_STREAM_LZSS_LENGTH_BITS
    /* A68 80010268 00000D24 */  addiu      $t5, $zero, 0x0
    /* A6C 8001026C 0C004015 */  bnez       $t2, .LfsStreamResumeMatchLength
    /* A70 80010270 00000000 */   nop
    FILE_SYSTEM_STREAM_LZSS_REFILL 20, .LfsStreamResumeMatchLength
  .LfsStreamResumeMatchLength:
    /* AA0 800102A0 2220C601 */   sub       $a0, $t6, $a2 /* handwritten instruction */
    /* AA4 800102A4 0E008018 */  blez       $a0, .LfsStreamFinishMatchLength
    /* AA8 800102A8 00000000 */   nop
    /* AAC 800102AC 21708000 */  addu       $t6, $a0, $zero
    /* AB0 800102B0 24684B01 */  and        $t5, $t2, $t3
    FILE_SYSTEM_STREAM_LZSS_REFILL 24, .LfsStreamFinishMatchLength
  .LfsStreamFinishMatchLength:
    /* AE0 800102E0 24284B01 */   and       $a1, $t2, $t3
    /* AE4 800102E4 0650CA01 */  srlv       $t2, $t2, $t6
    /* AE8 800102E8 2230CE00 */  sub        $a2, $a2, $t6 /* handwritten instruction */
    /* AEC 800102EC FF004439 */  xori       $a0, $t2, FILE_SYSTEM_STREAM_LZSS_BYTE_MASK
    /* AF0 800102F0 2428A400 */  and        $a1, $a1, $a0
    /* AF4 800102F4 0468CD01 */  sllv       $t5, $t5, $t6
    /* AF8 800102F8 0628C500 */  srlv       $a1, $a1, $a2
    /* AFC 800102FC 2568A501 */  or         $t5, $t5, $a1
    /* B00 80010300 0100A325 */  addiu      $v1, $t5, FILE_SYSTEM_STREAM_LZSS_MIN_MATCH_BYTES - 1
  .LfsStreamCopyMatch:
    /* Copy forward through the ring so a match may reuse bytes it just wrote. */
    /* B04 80010304 FF00E431 */  andi       $a0, $t7, FILE_SYSTEM_STREAM_LZSS_WINDOW_MASK
    /* B08 80010308 2110E400 */  addu       $v0, $a3, $a0
    /* B0C 8001030C 00004D90 */  lbu        $t5, 0x0($v0)
    /* B10 80010310 2110EC00 */  addu       $v0, $a3, $t4
    /* B14 80010314 00004DA0 */  sb         $t5, 0x0($v0)
    /* B18 80010318 00002DA1 */  sb         $t5, 0x0($t1)
    /* B1C 8001031C 01008C25 */  addiu      $t4, $t4, 0x1
    /* B20 80010320 FF008C31 */  andi       $t4, $t4, FILE_SYSTEM_STREAM_LZSS_WINDOW_MASK
    /* B24 80010324 01002925 */  addiu      $t1, $t1, 0x1
    /* B28 80010328 0100EF25 */  addiu      $t7, $t7, 0x1
    /* B2C 8001032C FFFF6320 */  addi       $v1, $v1, -0x1 /* handwritten instruction */
    /* B30 80010330 F4FF6104 */  bgez       $v1, .LfsStreamCopyMatch
    /* B34 80010334 00000000 */   nop
    /* B38 80010338 30400008 */  j          .LfsStreamReadTokenFlag
  .LfsStreamComplete:
    /* B3C 8001033C 01000424 */   addiu     $a0, $zero, FILE_SYSTEM_STREAM_DECODE_COMPLETE
    /* B40 80010340 0780013C */  lui        $at, %hi(D5B498_8006D748)
    /* B44 80010344 48D724A4 */  sh         $a0, %lo(D5B498_8006D748)($at)
  .LfsStreamSaveState:
    /* Save the last loaded input byte as well as the unfinished token. */
    /* B48 80010348 0780013C */  lui        $at, %hi(Fs_ChunkReadPtr)
    /* B4C 8001034C 2CC228AC */  sw         $t0, %lo(Fs_ChunkReadPtr)($at)
    /* B50 80010350 0780013C */  lui        $at, %hi(Fs_ChunkWritePtr)
    /* B54 80010354 D8D429AC */  sw         $t1, %lo(Fs_ChunkWritePtr)($at)
    /* B58 80010358 0780013C */  lui        $at, %hi(D5B498_8006EBB0)
    /* B5C 8001035C B0EB2AA4 */  sh         $t2, %lo(D5B498_8006EBB0)($at)
    /* B60 80010360 0780013C */  lui        $at, %hi(D5B498_8006EA1A)
    /* B64 80010364 1AEA2BA4 */  sh         $t3, %lo(D5B498_8006EA1A)($at)
    /* B68 80010368 0780013C */  lui        $at, %hi(D5B498_8006D858)
    /* B6C 8001036C 58D82CA4 */  sh         $t4, %lo(D5B498_8006D858)($at)
    /* B70 80010370 0780013C */  lui        $at, %hi(D5B498_8006D85A)
    /* B74 80010374 5AD826A4 */  sh         $a2, %lo(D5B498_8006D85A)($at)
    /* B78 80010378 0780013C */  lui        $at, %hi(D_8006EA08)
    /* B7C 8001037C 08EA2DA4 */  sh         $t5, %lo(D_8006EA08)($at)
    /* B80 80010380 0780013C */  lui        $at, %hi(D_8006EA0A)
    /* B84 80010384 0AEA2EA4 */  sh         $t6, %lo(D_8006EA0A)($at)
    /* B88 80010388 0780013C */  lui        $at, %hi(D_8006EA0C)
    /* B8C 8001038C 0CEA2FAC */  sw         $t7, %lo(D_8006EA0C)($at)
    /* B90 80010390 0800E003 */  jr         $ra
    /* B94 80010394 00000000 */   nop
endlabel fsDecompressStream
