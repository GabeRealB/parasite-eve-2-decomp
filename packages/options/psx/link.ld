/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(40);
        */options/src/package.o(.rodata);
        */options/src/package.o(.text);
        */options/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */options/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
