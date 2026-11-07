/* The disc's layout of p08_snail: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(8);
        */weapons/p08_snail/src/package.o(.rodata);
        */weapons/p08_snail/src/package.o(.text);
        */weapons/p08_snail/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/p08_snail/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
