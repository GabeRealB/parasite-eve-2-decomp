/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(62);
        */combat_light/src/package.o(.rodata);
        */combat_light/src/package.o(.text);
        */combat_light/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */combat_light/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
