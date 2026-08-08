.include "macro.inc"

/*
 * Overlay package header (entry/callbacks, demo strings, switch jtables).
 *
 * Linked as hasm with linker_section_order: .rodata so this block is first
 * in the flat OVR image (section_order: rodata → text → data). The linker
 * script pulls header.s.o(.text), so the payload lives in .text even though
 * it is constant data — same pattern as main's early hasm with
 * linker_section_order: .rodata.
 */
.section .text
.align 2

dlabel D_80093800
    /* 0 80093800 */ .word 0x00000005
    /* 4 80093804 — 5-way task dispatch table for func_8009472C */
dlabel D_80093804
    /* 4 80093804 */ .word func_8009389C
    /* 8 80093808 */ .word func_8009470C
    /* C 8009380C */ .word func_80093ABC
    /* 10 80093810 */ .word func_80093ABC
    /* 14 80093814 */ .word Task_Kill
    /* 18 80093818 */ .word 0x23232323
    /* 1C 8009381C */ .word 0x23232323
    /* 20 80093820 */ .word 0x45442323
    /* 24 80093824 */ .word 0x53204F4D
    /* 28 80093828 */ .word 0x54524154
    /* 2C 8009382C */ .word 0x0000000A
    /* 30 80093830 */ .word 0x23232323
    /* 34 80093834 */ .word 0x4F4D4544
    /* 38 80093838 */ .word 0x5241435F
    /* 3C 8009383C */ .word 0x45525F44
    /* 40 80093840 */ .word 0x524F5453
    /* 44 80093844 */ .word 0x54532045
    /* 48 80093848 */ .word 0x20454741
    /* 4C 8009384C */ .word 0x202C6425
    /* 50 80093850 */ .word 0x4E454353
    /* 54 80093854 */ .word 0x64252045
    /* 58 80093858 */ .word 0xE122000A
    /* 5C 8009385C — jtable for func_800947C8 */
    .word 0x80094810
    .word 0x80094840
    .word 0x800948B4
    .word 0x800948D4
    .word 0x80094914
    .word 0x80094968
    .word 0x8009498C
    .word 0x800949CC
    /* 7C 8009387C — jtable for func_80094A08 is compiler .rodata from title.c */
enddlabel D_80093800
