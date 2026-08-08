.include "macro.inc"

.set noat
.set noreorder

/*
 * Tmd_DispatchStream  (VRAM 0x80010A20 / ROM 0x1220)
 * ------------------------------------------------------------
 * Permanent handwritten assembly (splat type: hasm).
 *
 * Role
 *   Walk a TMD command stream (a2): for each record, unpack fields into
 *   the scratch block (a0) and jalr the per-entry handler until the
 *   terminator word -2. Called only from Tmd_SetupGteMatrices.
 *
 * Why this stays handwritten assembly
 *   Same early-image hasm constraints as Tmd_SetupGteMatrices (see that
 *   file). Small independent helper — kept as its own unit for clarity.
 *   Do not convert to type: c or INCLUDE_ASM from a regular TU.
 */

.section .text, "ax"

/* Walk stream records; jalr per-entry handler until terminator -2. */
glabel Tmd_DispatchStream
    /* 1220 80010A20 FCFFBD27 */  addiu      $sp, $sp, -0x4
    /* 1224 80010A24 0000A4AF */  sw         $a0, 0x0($sp)
    /* 1228 80010A28 300085AC */  sw         $a1, 0x30($a0)
    /* 122C 80010A2C 2C009FAC */  sw         $ra, 0x2C($a0)
  .L80010A30:
    /* 1230 80010A30 0000C88C */  lw         $t0, 0x0($a2)
    /* 1234 80010A34 FEFF0924 */  addiu      $t1, $zero, -0x2
    /* 1238 80010A38 10000911 */  beq        $t0, $t1, .L80010A7C
    /* 123C 80010A3C 3000858C */   lw        $a1, 0x30($a0)
    /* 1240 80010A40 0800CA8C */  lw         $t2, 0x8($a2)
    /* 1244 80010A44 0400C98C */  lw         $t1, 0x4($a2)
    /* 1248 80010A48 023C0A00 */  srl        $a3, $t2, 16
    /* 124C 80010A4C 00CC0A00 */  sll        $t9, $t2, 16
    /* 1250 80010A50 02CC1900 */  srl        $t9, $t9, 16
    /* 1254 80010A54 200088AC */  sw         $t0, 0x20($a0)
    /* 1258 80010A58 1C0087AC */  sw         $a3, 0x1C($a0)
    /* 125C 80010A5C 180099AC */  sw         $t9, 0x18($a0)
    /* 1260 80010A60 0C00C624 */  addiu      $a2, $a2, 0xC
    /* 1264 80010A64 09F82001 */  jalr       $t1
    /* 1268 80010A68 00000000 */   nop
    /* 126C 80010A6C 21300200 */  addu       $a2, $zero, $v0
    /* 1270 80010A70 0000A48F */  lw         $a0, 0x0($sp)
    /* 1274 80010A74 8C420008 */  j          .L80010A30
    /* 1278 80010A78 00000000 */   nop
  .L80010A7C:
    /* 127C 80010A7C 2C009F8C */  lw         $ra, 0x2C($a0)
    /* 1280 80010A80 21100600 */  addu       $v0, $zero, $a2
    /* 1284 80010A84 0400BD27 */  addiu      $sp, $sp, 0x4
    /* 1288 80010A88 0800E003 */  jr         $ra
    /* 128C 80010A8C 00000000 */   nop
endlabel Tmd_DispatchStream
