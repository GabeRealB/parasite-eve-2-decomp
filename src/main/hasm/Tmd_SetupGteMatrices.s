.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_SetupGteMatrices  (VRAM 0x80010848 / ROM 0x1048)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 *
 * Role
 *   Draw-path helper called from Tmd_SetupDraw. Loads light / colour
 *   matrices into the GTE (ctc2 to control regs) and runs packed mvmva
 *   vertex transforms for the current TMD node. Args (a0..a3) are the
 *   scratch draw block, flags, stream pointer, and TmdObject*.
 *   Calls Tmd_DispatchStream to walk the command stream.
 *
 * Why this stays handwritten assembly
 *   1. Direct GTE coprocessor ops (ctc2 / mtc2 / mfc2 / mvmva) laid out
 *      by hand with delay-slot packing; matching that schedule from C
 *      GTE macros is unreliable on this toolchain.
 *   2. Early-image placement (linker_section_order: .rodata) with the
 *      other boot helpers — not a normal .text TU.
 *   3. Spimdisasm marks several instructions handwritten; keep the
 *      original encoding forever.
 *   Do not convert to type: c or INCLUDE_ASM from a regular TU.
 */

.section .text, "ax"

glabel Tmd_SetupGteMatrices
    /* 1048 80010848 ECFFBD27 */  addiu      $sp, $sp, -0x14
    /* 104C 8001084C 0000B0AF */  sw         $s0, 0x0($sp)
    /* 1050 80010850 0400B1AF */  sw         $s1, 0x4($sp)
    /* 1054 80010854 0800B2AF */  sw         $s2, 0x8($sp)
    /* 1058 80010858 0C00B3AF */  sw         $s3, 0xC($sp)
    /* 105C 8001085C 1000BFAF */  sw         $ra, 0x10($sp)
    /* 1060 80010860 3000F08C */  lw         $s0, 0x30($a3)
    /* 1064 80010864 0800F18C */  lw         $s1, 0x8($a3)
    /* 1068 80010868 50009224 */  addiu      $s2, $a0, 0x50
    /* 106C 8001086C 20420008 */  j          .L80010880
    /* 1070 80010870 00000000 */   nop
  .L80010874:
    /* 1074 80010874 0400C624 */  addiu      $a2, $a2, 0x4
    /* 1078 80010878 FFFF1026 */  addiu      $s0, $s0, -0x1
    /* 107C 8001087C 50003126 */  addiu      $s1, $s1, 0x50
  .L80010880:
    /* 1080 80010880 0000C88C */  lw         $t0, 0x0($a2)
    /* 1084 80010884 FFFF0924 */  addiu      $t1, $zero, -0x1
    /* 1088 80010888 5D000911 */  beq        $t0, $t1, .L80010A00
    /* 108C 8001088C 00000000 */   nop
    /* 1090 80010890 5400001A */  blez       $s0, .L800109E4
    /* 1094 80010894 00000000 */   nop
    /* 1098 80010898 FEFF0924 */  addiu      $t1, $zero, -0x2
    /* 109C 8001089C 51000911 */  beq        $t0, $t1, .L800109E4
    /* 10A0 800108A0 00000000 */   nop
    /* 10A4 800108A4 2400288E */  lw         $t0, 0x24($s1)
    /* 10A8 800108A8 2800298E */  lw         $t1, 0x28($s1)
    /* 10AC 800108AC 2C002A8E */  lw         $t2, 0x2C($s1)
    /* 10B0 800108B0 30002B8E */  lw         $t3, 0x30($s1)
    /* 10B4 800108B4 0000C848 */  ctc2       $t0, $0 /* handwritten instruction */
    /* 10B8 800108B8 0008C948 */  ctc2       $t1, $1 /* handwritten instruction */
    /* 10BC 800108BC 0010CA48 */  ctc2       $t2, $2 /* handwritten instruction */
    /* 10C0 800108C0 0018CB48 */  ctc2       $t3, $3 /* handwritten instruction */
    /* 10C4 800108C4 FFFF1924 */  addiu      $t9, $zero, -0x1
    /* 10C8 800108C8 02CC1900 */  srl        $t9, $t9, 16
    /* 10CC 800108CC 00C41900 */  sll        $t8, $t9, 16
    /* 10D0 800108D0 24601901 */  and        $t4, $t0, $t9
    /* 10D4 800108D4 24683801 */  and        $t5, $t1, $t8
    /* 10D8 800108D8 2560AC01 */  or         $t4, $t5, $t4
    /* 10DC 800108DC 24687901 */  and        $t5, $t3, $t9
    /* 10E0 800108E0 24701801 */  and        $t6, $t0, $t8
    /* 10E4 800108E4 2568CD01 */  or         $t5, $t6, $t5
    /* 10E8 800108E8 24705901 */  and        $t6, $t2, $t9
    /* 10EC 800108EC 24787801 */  and        $t7, $t3, $t8
    /* 10F0 800108F0 2570EE01 */  or         $t6, $t7, $t6
    /* 10F4 800108F4 24783901 */  and        $t7, $t1, $t9
    /* 10F8 800108F8 24585801 */  and        $t3, $t2, $t8
    /* 10FC 800108FC 25786F01 */  or         $t7, $t3, $t7
    /* 1100 80010900 3400288E */  lw         $t0, 0x34($s1)
    /* 1104 80010904 0040CC48 */  ctc2       $t4, $8 /* handwritten instruction */
    /* 1108 80010908 0048CD48 */  ctc2       $t5, $9 /* handwritten instruction */
    /* 110C 8001090C 0050CE48 */  ctc2       $t6, $10 /* handwritten instruction */
    /* 1110 80010910 0058CF48 */  ctc2       $t7, $11 /* handwritten instruction */
    /* 1114 80010914 0020C848 */  ctc2       $t0, $4 /* handwritten instruction */
    /* 1118 80010918 0060C848 */  ctc2       $t0, $12 /* handwritten instruction */
    /* 111C 8001091C 0000488E */  lw         $t0, 0x0($s2)
    /* 1120 80010920 0400498E */  lw         $t1, 0x4($s2)
    /* 1124 80010924 00008848 */  mtc2       $t0, $0 /* handwritten instruction */
    /* 1128 80010928 00088948 */  mtc2       $t1, $1 /* handwritten instruction */
    /* 112C 8001092C 0800488E */  lw         $t0, 0x8($s2)
    /* 1130 80010930 024C0900 */  srl        $t1, $t1, 16
    /* 1134 80010934 12604A4A */  mvmva      1, 1, 0, 3, 0
    /* 1138 80010938 00540800 */  sll        $t2, $t0, 16
    /* 113C 8001093C 25484901 */  or         $t1, $t2, $t1
    /* 1140 80010940 02440800 */  srl        $t0, $t0, 16
    /* 1144 80010944 00008948 */  mtc2       $t1, $0 /* handwritten instruction */
    /* 1148 80010948 00088848 */  mtc2       $t0, $1 /* handwritten instruction */
    /* 114C 8001094C 00480D48 */  mfc2       $t5, $9 /* handwritten instruction */
    /* 1150 80010950 00500E48 */  mfc2       $t6, $10 /* handwritten instruction */
    /* 1154 80010954 00580F48 */  mfc2       $t7, $11 /* handwritten instruction */
    /* 1158 80010958 2468B901 */  and        $t5, $t5, $t9
    /* 115C 8001095C 00740E00 */  sll        $t6, $t6, 16
    /* 1160 80010960 2558CD01 */  or         $t3, $t6, $t5
    /* 1164 80010964 12604A4A */  mvmva      1, 1, 0, 3, 0
    /* 1168 80010968 0C00488E */  lw         $t0, 0xC($s2)
    /* 116C 8001096C 1000498E */  lw         $t1, 0x10($s2)
    /* 1170 80010970 00008848 */  mtc2       $t0, $0 /* handwritten instruction */
    /* 1174 80010974 00088948 */  mtc2       $t1, $1 /* handwritten instruction */
    /* 1178 80010978 00480C48 */  mfc2       $t4, $9 /* handwritten instruction */
    /* 117C 8001097C 00500D48 */  mfc2       $t5, $10 /* handwritten instruction */
    /* 1180 80010980 00580E48 */  mfc2       $t6, $11 /* handwritten instruction */
    /* 1184 80010984 2478F901 */  and        $t7, $t7, $t9
    /* 1188 80010988 00640C00 */  sll        $t4, $t4, 16
    /* 118C 8001098C 2560EC01 */  or         $t4, $t7, $t4
    /* 1190 80010990 2468B901 */  and        $t5, $t5, $t9
    /* 1194 80010994 00740E00 */  sll        $t6, $t6, 16
    /* 1198 80010998 2568CD01 */  or         $t5, $t6, $t5
    /* 119C 8001099C 12604A4A */  mvmva      1, 1, 0, 3, 0
    /* 11A0 800109A0 00480E48 */  mfc2       $t6, $9 /* handwritten instruction */
    /* 11A4 800109A4 00500F48 */  mfc2       $t7, $10 /* handwritten instruction */
    /* 11A8 800109A8 00581848 */  mfc2       $t8, $11 /* handwritten instruction */
    /* 11AC 800109AC 2470D901 */  and        $t6, $t6, $t9
    /* 11B0 800109B0 007C0F00 */  sll        $t7, $t7, 16
    /* 11B4 800109B4 2570EE01 */  or         $t6, $t7, $t6
    /* 11B8 800109B8 0040CB48 */  ctc2       $t3, $8 /* handwritten instruction */
    /* 11BC 800109BC 0048CC48 */  ctc2       $t4, $9 /* handwritten instruction */
    /* 11C0 800109C0 0050CD48 */  ctc2       $t5, $10 /* handwritten instruction */
    /* 11C4 800109C4 0058CE48 */  ctc2       $t6, $11 /* handwritten instruction */
    /* 11C8 800109C8 0060D848 */  ctc2       $t8, $12 /* handwritten instruction */
    /* 11CC 800109CC 3800288E */  lw         $t0, 0x38($s1)
    /* 11D0 800109D0 3C00298E */  lw         $t1, 0x3C($s1)
    /* 11D4 800109D4 40002A8E */  lw         $t2, 0x40($s1)
    /* 11D8 800109D8 0028C848 */  ctc2       $t0, $5 /* handwritten instruction */
    /* 11DC 800109DC 0030C948 */  ctc2       $t1, $6 /* handwritten instruction */
    /* 11E0 800109E0 0038CA48 */  ctc2       $t2, $7 /* handwritten instruction */
  .L800109E4:
    /* 11E4 800109E4 21980700 */  addu       $s3, $zero, $a3
    /* 11E8 800109E8 8842000C */  jal        Tmd_DispatchStream
    /* 11EC 800109EC 00000000 */   nop
    /* 11F0 800109F0 21300200 */  addu       $a2, $zero, $v0
    /* 11F4 800109F4 21381300 */  addu       $a3, $zero, $s3
    /* 11F8 800109F8 1D420008 */  j          .L80010874
    /* 11FC 800109FC 00000000 */   nop
  .L80010A00:
    /* 1200 80010A00 1000BF8F */  lw         $ra, 0x10($sp)
    /* 1204 80010A04 0C00B38F */  lw         $s3, 0xC($sp)
    /* 1208 80010A08 0800B28F */  lw         $s2, 0x8($sp)
    /* 120C 80010A0C 0400B18F */  lw         $s1, 0x4($sp)
    /* 1210 80010A10 0000B08F */  lw         $s0, 0x0($sp)
    /* 1214 80010A14 1400BD27 */  addiu      $sp, $sp, 0x14
    /* 1218 80010A18 0800E003 */  jr         $ra
    /* 121C 80010A1C 00000000 */   nop
endlabel Tmd_SetupGteMatrices
