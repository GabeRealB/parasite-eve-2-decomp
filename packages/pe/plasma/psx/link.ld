/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(52);
        */pe/plasma/src/package.o(.rodata);
        */pe/plasma/src/package.o(.text);
        */pe/plasma/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/plasma/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
