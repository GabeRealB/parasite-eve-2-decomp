/* The disc's layout of this package: its id, its sections in the disc's order, at the overlay slot, where a Parasite Energy or an item cast like one loads. */
SLOT = 0x8012EF30;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(54);
        */pe/metabolism/src/package.o(.rodata);
        */pe/metabolism/src/package.o(.text);
        */pe/metabolism/src/package.o(.data);
        */pe/metabolism/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
