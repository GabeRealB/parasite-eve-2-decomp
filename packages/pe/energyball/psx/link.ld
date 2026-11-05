/* The disc's layout of this package: its id, its sections in the disc's order, at the overlay slot, where a Parasite Energy or an item cast like one loads. */
SLOT = 0x8012EF30;
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(59);
        */pe/energyball/src/package.o(.rodata);
        */pe/energyball/src/package.o(.text);
        */pe/energyball/src/package.o(.data);
        */pe/energyball/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
