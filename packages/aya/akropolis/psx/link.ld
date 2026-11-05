/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(0x0000002C);
        */aya/akropolis/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4)
    {
        */aya/akropolis/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
