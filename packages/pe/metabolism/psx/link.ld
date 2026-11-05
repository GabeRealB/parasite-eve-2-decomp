/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
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
