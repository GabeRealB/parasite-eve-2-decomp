/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(55);
        */pe/healing/src/package.o(.rodata);
        */pe/healing/src/package.o(.text);
        */pe/healing/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/healing/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
