/* The disc's layout of gunblade: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(30);
        */weapons/gunblade/src/package.o(.rodata);
        */weapons/gunblade/src/package.o(.text);
        */weapons/gunblade/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/gunblade/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
