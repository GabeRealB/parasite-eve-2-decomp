/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(57);
        */pe/antibody/src/package.o(.rodata);
        */pe/antibody/src/package.o(.text);
        */pe/antibody/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/antibody/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
