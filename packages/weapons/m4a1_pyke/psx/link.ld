/* The disc's layout of m4a1_pyke: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(35);
        */weapons/m4a1_pyke/src/package.o(.rodata);
        */weapons/m4a1_pyke/src/package.o(.text);
        */weapons/m4a1_pyke/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/m4a1_pyke/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
