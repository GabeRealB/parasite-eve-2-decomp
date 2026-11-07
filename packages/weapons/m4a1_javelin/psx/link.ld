/* The disc's layout of m4a1_javelin: its id, its sections in the disc's order, at the weapon slot, where the equipped weapon loads. */
SLOT = 0x8011D1C0;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(36);
        */weapons/m4a1_javelin/src/package.o(.rodata);
        */weapons/m4a1_javelin/src/package.o(.text);
        */weapons/m4a1_javelin/src/package.o(.data);
        /* The alignment gap before the stored BSS holds whatever the packaging tool's buffer did: this on the USA
           disc, 0xE3A93F2C on the European, zero on the Japanese. */
        LONG(0x102232DD);
        */weapons/m4a1_javelin/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
