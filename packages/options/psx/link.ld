/* The disc's layout of this package: its id, its sections in the disc's order, at the options slot, where the options menu loads, and the package after it. */
SLOT = 0x801D4000;
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
