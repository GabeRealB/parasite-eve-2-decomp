/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(0x0000002E);
        */aya_mist/src/aya_mist.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4)
    {
        */aya_mist/src/aya_mist.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
