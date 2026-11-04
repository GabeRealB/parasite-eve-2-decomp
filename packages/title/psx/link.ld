/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
PROVIDE(func_807246B4 = 0x807246B4);
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(5);
        *title.o(.rodata);
        *(.rodata);
        *title.o(.text);
        *(.text);
        *title.o(.data);
        *(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { *title.o(.bss); *(.bss); }
    /DISCARD/ : { *(*); }
}
