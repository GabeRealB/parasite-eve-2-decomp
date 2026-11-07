/* The disc's layout of tonfa_baton: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(26);
        */weapons/tonfa_baton/src/package.o(.rodata);
        */weapons/tonfa_baton/src/package.o(.text);
        */weapons/tonfa_baton/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/tonfa_baton/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
