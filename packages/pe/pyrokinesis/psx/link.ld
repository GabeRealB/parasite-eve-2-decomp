/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(48);
        */pe/pyrokinesis/src/package.o(.rodata);
        */pe/pyrokinesis/src/package.o(.text);
        */pe/pyrokinesis/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/pyrokinesis/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
