/* The disc's layout of grenade_pistol: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(18);
        */weapons/grenade_pistol/src/package.o(.rodata);
        */weapons/grenade_pistol/src/package.o(.text);
        */weapons/grenade_pistol/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */weapons/grenade_pistol/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
