/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(51);
        */pe/necrosis/src/package.o(.rodata);
        */pe/necrosis/src/package.o(.text);
        */pe/necrosis/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/necrosis/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
