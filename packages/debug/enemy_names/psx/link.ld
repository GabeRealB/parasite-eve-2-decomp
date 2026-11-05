/* The disc's layout of this package: its id, its sections in the disc's order, at the debug slot. */
SLOT = 0x801D6000;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        */enemy_names/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */enemy_names/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
