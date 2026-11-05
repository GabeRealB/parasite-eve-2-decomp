/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
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
