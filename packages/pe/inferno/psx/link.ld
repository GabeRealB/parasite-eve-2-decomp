/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(50);
        */pe/inferno/src/package.o(.rodata);
        */pe/inferno/src/package.o(.text);
        */pe/inferno/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/inferno/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
