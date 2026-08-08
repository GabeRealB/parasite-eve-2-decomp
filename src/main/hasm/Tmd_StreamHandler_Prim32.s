.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_StreamHandler_Prim32  (VRAM 0x800105cc / ROM 0xdcc)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * TMD stream handler (function pointer from Tmd_InitSourceStream).
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
    /* DEC 800105EC */  j           .L80010624
    /* DF0 800105F0 */  lw          $a1, 0x84($a0)
alabel Tmd_StreamHandler_Prim30
    /* DF4 800105F4 */  lw          $t9, 0x18($a0)
    /* DF8 800105F8 */  lw          $a3, 0x1C($a0)
    /* DFC 800105FC */  lw          $t8, 0x4($a0)
    /* E00 80010600 */  lw          $t7, 0x14($a0)
    /* E04 80010604 */  lw          $t6, 0x10($a0)
    /* E08 80010608 */  sll         $t9, $t9, 2
    /* E0C 8001060C */  lui         $v0, 0x600
    /* E10 80010610 */  addiu       $v1, $zero, 0x30
    /* E14 80010614 */  j           .L80010624
    /* E18 80010618 */  lw          $a1, 0x84($a0)
  .L8001061C:
    /* E1C 8001061C */  addiu       $t8, $t8, 0x1C
    /* E20 80010620 */  addu        $a2, $t9, $a2
  .L80010624:
    /* E24 80010624 */  beq         $zero, $a3, .L800106E0
    /* E28 80010628 */  nop
    /* E2C 8001062C */  lwc2        $15, 0x8($t8)
    /* E30 80010630 */  lwc2        $15, 0x10($t8)
    /* E34 80010634 */  lwc2        $15, 0x18($t8)
    /* E38 80010638 */  addiu       $a3, $a3, -0x1
    /* E3C 8001063C */  nop
    /* E40 80010640 */  .word 0x4B400006
    /* E44 80010644 */  lw          $t1, 0x0($a2)
    /* E48 80010648 */  lw          $t3, 0x4($a2)
    /* E4C 8001064C */  mfc2        $t0, $24
    /* E50 80010650 */  srl         $t2, $t1, 16
    /* E54 80010654 */  blez        $t0, .L8001061C
    /* E58 80010658 */  addu        $t2, $t6, $t2
    /* E5C 8001065C */  lw          $t2, 0x0($t2)
    /* E60 80010660 */  sll         $t1, $t1, 16
    /* E64 80010664 */  bltz        $t2, .L8001061C
    /* E68 80010668 */  srl         $t1, $t1, 16
    /* E6C 8001066C */  addu        $t1, $t6, $t1
    /* E70 80010670 */  lw          $t1, 0x0($t1)
    /* E74 80010674 */  sll         $t3, $t3, 16
    /* E78 80010678 */  bltz        $t1, .L8001061C
    /* E7C 8001067C */  srl         $t3, $t3, 16
    /* E80 80010680 */  addu        $t3, $t6, $t3
    /* E84 80010684 */  lw          $t3, 0x0($t3)
    /* E88 80010688 */  mtc2        $t1, $17
    /* E8C 8001068C */  bltz        $t3, .L8001061C
    /* E90 80010690 */  mtc2        $t2, $18
    /* E94 80010694 */  mtc2        $t3, $19
    /* E98 80010698 */  lui         $t0, 0xFF
    /* E9C 8001069C */  ori         $t0, $t0, 0xFFFF
    /* EA0 800106A0 */  .word 0x4B58002D
    /* EA4 800106A4 */  and         $t1, $t0, $t8
    /* EA8 800106A8 */  mfc2        $t2, $7
    /* EAC 800106AC */  sb          $v1, 0x7($t8)
    /* EB0 800106B0 */  sllv        $t2, $t2, $a1
    /* EB4 800106B4 */  andi        $t2, $t2, 0x3FFF
    /* EB8 800106B8 */  srl         $t2, $t2, 4
    /* EBC 800106BC */  sll         $t2, $t2, 2
    /* EC0 800106C0 */  addu        $t2, $t2, $t7
    /* EC4 800106C4 */  lw          $t5, 0x0($t2)
    /* EC8 800106C8 */  sw          $t1, 0x0($t2)
    /* ECC 800106CC */  and         $t5, $t0, $t5
    /* ED0 800106D0 */  or          $t5, $v0, $t5
    /* ED4 800106D4 */  sw          $t5, 0x0($t8)
    /* ED8 800106D8 */  j           .L8001061C
    /* EDC 800106DC */  nop
  .L800106E0:
    /* EE0 800106E0 */  sw          $t8, 0x4($a0)
    /* EE4 800106E4 */  addu        $v0, $zero, $a2
    /* EE8 800106E8 */  jr          $ra
    /* EEC 800106EC */  nop
endlabel Tmd_StreamHandler_Prim32
