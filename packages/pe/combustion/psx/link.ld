/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(49);
        */pe/combustion/src/package.o(.rodata);
        */pe/combustion/src/package.o(.text);
        */pe/combustion/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/combustion/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
