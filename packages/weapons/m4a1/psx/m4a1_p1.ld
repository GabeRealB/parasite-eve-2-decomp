/* The disc's layout of m4a1_p1: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(27);
        */weapons/m4a1_p1/src/package.o(.rodata);
        */weapons/m4a1_p1/src/package.o(.text);
        */weapons/m4a1_p1/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/m4a1_p1/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
