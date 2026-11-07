/* The disc's layout of mongoose: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(16);
        */weapons/mongoose/src/package.o(.rodata);
        */weapons/mongoose/src/package.o(.text);
        */weapons/mongoose/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/mongoose/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
