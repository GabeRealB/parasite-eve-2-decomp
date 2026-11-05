/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
PROVIDE(func_807246B4 = 0x807246B4);
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(5);
        */title/src/package.o(.rodata);
        *(.rodata);
        */title/src/package.o(.text);
        *(.text);
        */title/src/package.o(.data);
        *(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */title/src/package.o(.bss); *(.bss); }
    /DISCARD/ : { *(*); }
}
