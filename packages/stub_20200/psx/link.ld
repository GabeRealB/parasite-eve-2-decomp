/* The disc's layout of this package: its id, its sections in the disc's order. SLOT is the target's. */
SECTIONS
{
    .image SLOT : SUBALIGN(4)
    {
        LONG(42);
    }
    /DISCARD/ : { *(*); }
}
