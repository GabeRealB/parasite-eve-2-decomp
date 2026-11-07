/* The disc's layout of p229: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(12);
        */weapons/p229/src/package.o(.rodata);
        */weapons/p229/src/package.o(.text);
        */weapons/p229/src/package.o(.data);
        */weapons/p229/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
