.include "macro.inc"

.set noat
.set noreorder

/*
 * tmdSkipStreamRecord  (VRAM 0x800105ac / ROM 0xdac)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 * Role: fallback when the current pass has no handler for a stream record.
 * The caller has stored elemStride and elemCount and passed the first element
 * word; this returns that cursor advanced by stride times count words.
 * Documented at its declaration in src/main/tmd.h.
 * Early-image placement (linker_section_order: .rodata).
 */

.section .text, "ax"

glabel tmdSkipStreamRecord
    /* DAC 800105AC */  lw          $t9, 0x18($a0)          /* elemStride, in words */
    /* DB0 800105B0 */  lw          $a3, 0x1C($a0)          /* elemCount */
    /* DB4 800105B4 */  sll         $t9, $t9, 2
    /* DB8 800105B8 */  multu       $t9, $a3
    /* DBC 800105BC */  mflo        $t9
    /* DC0 800105C0 */  addu        $v0, $t9, $a2           /* element cursor plus stride * count words */
    /* DC4 800105C4 */  jr          $ra
    /* DC8 800105C8 */  nop
endlabel tmdSkipStreamRecord
