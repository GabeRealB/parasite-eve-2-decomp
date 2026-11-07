/* The disc's layout of mp5a5: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(37);
        */weapons/mp5a5/src/package.o(.rodata);
        */weapons/mp5a5/src/package.o(.text);
        */weapons/mp5a5/src/package.o(.data);
        */weapons/mp5a5/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
