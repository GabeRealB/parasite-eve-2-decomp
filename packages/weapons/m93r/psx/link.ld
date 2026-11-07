/* The disc's layout of this package: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(9);
        */weapons/m93r/src/package.o(.rodata);
        */weapons/m93r/src/package.o(.text);
        */weapons/m93r/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/m93r/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
