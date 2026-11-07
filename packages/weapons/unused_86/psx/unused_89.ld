/* The disc's layout of unused_89: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(17);
        */weapons/unused_89/src/package.o(.rodata);
        */weapons/unused_89/src/package.o(.text);
        */weapons/unused_89/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/unused_89/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
