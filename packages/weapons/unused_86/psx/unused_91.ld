/* The disc's layout of unused_91: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(25);
        */weapons/unused_91/src/package.o(.rodata);
        */weapons/unused_91/src/package.o(.text);
        */weapons/unused_91/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/unused_91/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
