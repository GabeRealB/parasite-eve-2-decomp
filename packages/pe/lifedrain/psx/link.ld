/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(56);
        */pe/lifedrain/src/package.o(.rodata);
        */pe/lifedrain/src/package.o(.text);
        */pe/lifedrain/src/package.o(.data);
    }
    .unstored (NOLOAD) : SUBALIGN(4) { */pe/lifedrain/src/package.o(.bss); }
    /DISCARD/ : { *(*); }
}
