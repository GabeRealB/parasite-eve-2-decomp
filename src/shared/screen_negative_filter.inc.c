/* Part of the screen negative library; see screen_negative.h. */

/// Walks the frame two GPU words (four 15-bit pixels) at a time, covering
/// every pixel. For each pixel it forms the luma
/// (3R + 4G + B) / 8, inverts it (31 - l) and writes it back to all three
/// channels.
static void screenNegativeFilter(void)
{
    s32     i;
    u_long* p0;
    u_long* p1;
    u32     hi;
    u32     lo;
    u32     gray;
    u32     t;

    p0 = Fs_ImgBuffers->strips[0];
    i  = 0;
    p1 = p0 + 1;
    do {
        i++;
        hi    = *p1;
        lo    = *p0;
        t     = hi & 0x001F001F;
        t   <<= 8;
        t    |= lo & 0x001F001F;
        gray  = t * 3;
        t     = hi & 0x03E003E0;
        t   <<= 3;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t * 4;
        hi  >>= 2;
        t     = hi & 0x1F001F00;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t;
        gray  = (gray >> 3) & 0x1F1F1F1F;
        gray  = 0x1F1F1F1F - gray;

        lo   = gray & 0x001F001F;
        lo  |= (lo << 10) | (lo << 5);
        hi   = gray & 0x1F001F00;
        hi >>= 8;
        hi  |= (hi << 10) | (hi << 5);
        *p0  = lo;
        *p1  = hi;
        p1  += 2;
        p0  += 2;
    } while (i < (FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS) / 2);
}
