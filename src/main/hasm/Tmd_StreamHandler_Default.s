.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_StreamHandler_Default  (VRAM 0x800105ac / ROM 0xdac)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * TMD stream handler (function pointer from Tmd_InitSourceStream).
 * Early-image placement (linker_section_order: .rodata).
 */

.section .text, "ax"

glabel Tmd_StreamHandler_Default
    /* DAC 800105AC */  lw          $t9, 0x18($a0)
    /* DB0 800105B0 */  lw          $a3, 0x1C($a0)
    /* DB4 800105B4 */  sll         $t9, $t9, 2
    /* DB8 800105B8 */  multu       $t9, $a3
    /* DBC 800105BC */  mflo        $t9
    /* DC0 800105C0 */  addu        $v0, $t9, $a2
    /* DC4 800105C4 */  jr          $ra
    /* DC8 800105C8 */  nop
endlabel Tmd_StreamHandler_Default
