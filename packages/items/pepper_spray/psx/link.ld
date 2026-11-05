/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(61);
        */pepper_spray/src/package.o(.rodata);
        */pepper_spray/src/package.o(.text);
        */pepper_spray/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pepper_spray/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
