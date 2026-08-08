.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_StreamHandler_Prim3A  (VRAM 0x800106f0 / ROM 0xef0)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * TMD stream handler (function pointer from Tmd_InitSourceStream).
 * Early-image placement (linker_section_order: .rodata).
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
    /* F10 80010710 */  j           .L80010748
    /* F14 80010714 */  lw          $a1, 0x84($a0)
alabel Tmd_StreamHandler_Prim38
    /* F18 80010718 */  lw          $t9, 0x18($a0)
    /* F1C 8001071C */  lw          $a3, 0x1C($a0)
    /* F20 80010720 */  lw          $t8, 0x4($a0)
    /* F24 80010724 */  lw          $t7, 0x14($a0)
    /* F28 80010728 */  lw          $t6, 0x10($a0)
    /* F2C 8001072C */  sll         $t9, $t9, 2
    /* F30 80010730 */  lui         $v0, 0x800
    /* F34 80010734 */  addiu       $v1, $zero, 0x38
    /* F38 80010738 */  j           .L80010748
    /* F3C 8001073C */  lw          $a1, 0x84($a0)
  .L80010740:
    /* F40 80010740 */  addiu       $t8, $t8, 0x24
    /* F44 80010744 */  addu        $a2, $t9, $a2
  .L80010748:
    /* F48 80010748 */  beq         $zero, $a3, .L80010838
    /* F4C 8001074C */  nop
    /* F50 80010750 */  lwc2        $15, 0x8($t8)
    /* F54 80010754 */  lwc2        $15, 0x10($t8)
    /* F58 80010758 */  lwc2        $15, 0x18($t8)
    /* F5C 8001075C */  addiu       $a3, $a3, -0x1
    /* F60 80010760 */  nop
    /* F64 80010764 */  .word 0x4B400006
    /* F68 80010768 */  lw          $t1, 0x0($a2)
    /* F6C 8001076C */  lw          $t3, 0x4($a2)
    /* F70 80010770 */  mfc2        $t0, $24
    /* F74 80010774 */  srl         $t2, $t1, 16
    /* F78 80010778 */  bgtz        $t0, .L8001079C
    /* F7C 8001077C */  nop
    /* F80 80010780 */  lwc2        $15, 0x20($t8)
    /* F84 80010784 */  nop
    /* F88 80010788 */  nop
    /* F8C 8001078C */  .word 0x4B400006
    /* F90 80010790 */  mfc2        $t0, $24
    /* F94 80010794 */  nop
    /* F98 80010798 */  bgez        $t0, .L80010740
  .L8001079C:
    /* F9C 8001079C */  addu        $t2, $t6, $t2
    /* FA0 800107A0 */  lw          $t2, 0x0($t2)
    /* FA4 800107A4 */  sll         $t1, $t1, 16
    /* FA8 800107A8 */  bltz        $t2, .L80010740
    /* FAC 800107AC */  srl         $t1, $t1, 16
    /* FB0 800107B0 */  addu        $t1, $t6, $t1
    /* FB4 800107B4 */  lw          $t1, 0x0($t1)
    /* FB8 800107B8 */  srl         $t4, $t3, 16
    /* FBC 800107BC */  bltz        $t1, .L80010740
    /* FC0 800107C0 */  addu        $t4, $t6, $t4
    /* FC4 800107C4 */  lw          $t4, 0x0($t4)
    /* FC8 800107C8 */  sll         $t3, $t3, 16
    /* FCC 800107CC */  bltz        $t4, .L80010740
    /* FD0 800107D0 */  srl         $t3, $t3, 16
    /* FD4 800107D4 */  addu        $t3, $t6, $t3
    /* FD8 800107D8 */  lw          $t3, 0x0($t3)
    /* FDC 800107DC */  mtc2        $t1, $16
    /* FE0 800107E0 */  bltz        $t3, .L80010740
    /* FE4 800107E4 */  mtc2        $t2, $17
    /* FE8 800107E8 */  mtc2        $t3, $18
    /* FEC 800107EC */  mtc2        $t4, $19
    /* FF0 800107F0 */  lui         $t0, 0xFF
    /* FF4 800107F4 */  ori         $t0, $t0, 0xFFFF
    /* FF8 800107F8 */  .word 0x4B68002E
    /* FFC 800107FC */  and         $t1, $t0, $t8
    /* 1000 80010800 */  mfc2        $t2, $7
    /* 1004 80010804 */  sb          $v1, 0x7($t8)
    /* 1008 80010808 */  sllv        $t2, $t2, $a1
    /* 100C 8001080C */  andi        $t2, $t2, 0x3FFF
    /* 1010 80010810 */  srl         $t2, $t2, 4
    /* 1014 80010814 */  sll         $t2, $t2, 2
    /* 1018 80010818 */  addu        $t2, $t2, $t7
    /* 101C 8001081C */  lw          $t5, 0x0($t2)
    /* 1020 80010820 */  sw          $t1, 0x0($t2)
    /* 1024 80010824 */  and         $t5, $t0, $t5
    /* 1028 80010828 */  or          $t5, $v0, $t5
    /* 102C 8001082C */  sw          $t5, 0x0($t8)
    /* 1030 80010830 */  j           .L80010740
    /* 1034 80010834 */  nop
  .L80010838:
    /* 1038 80010838 */  sw          $t8, 0x4($a0)
    /* 103C 8001083C */  addu        $v0, $zero, $a2
    /* 1040 80010840 */  jr          $ra
    /* 1044 80010844 */  nop
endlabel Tmd_StreamHandler_Prim3A
