/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(58);
        */pe/energyshot/src/package.o(.rodata);
        */pe/energyshot/src/package.o(.text);
        */pe/energyshot/src/package.o(.data);
        */pe/energyshot/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
