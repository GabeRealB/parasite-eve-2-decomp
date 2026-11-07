/* The disc's layout of mm1: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(19);
        */weapons/mm1/src/package.o(.rodata);
        */weapons/mm1/src/package.o(.text);
        */weapons/mm1/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/mm1/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
