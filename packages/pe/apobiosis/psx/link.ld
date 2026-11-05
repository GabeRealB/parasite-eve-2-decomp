/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(53);
        */pe/apobiosis/src/package.o(.rodata);
        */pe/apobiosis/src/package.o(.text);
        */pe/apobiosis/src/package.o(.data);
        */pe/apobiosis/src/package.o(.bss);
    }
    /DISCARD/ : { *(*); }
}
