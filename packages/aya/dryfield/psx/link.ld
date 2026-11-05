/* The disc's layout of this package: its id, its sections in the disc's order, at Aya's slot, where her costume loads. */
SLOT = 0x80115770;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(0x00000007);
        */aya/dryfield/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4)
    {
        */aya/dryfield/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
