/* The disc's layout of this package: its id, its sections in the disc's order, at the overlay slot, where a Parasite Energy or an item cast like one loads. */
SLOT = 0x8012EF30;
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
