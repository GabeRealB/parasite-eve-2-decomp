/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        */enemy_names/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */enemy_names/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
