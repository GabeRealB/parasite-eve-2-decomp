/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(60);
        */flare/src/package.o(.rodata);
        */flare/src/package.o(.text);
        */flare/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */flare/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
