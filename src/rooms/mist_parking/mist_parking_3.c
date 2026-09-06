#include "common.h"

#include <psyq/libgte.h>

#include "main/text.h"

extern u8 D_mist_parking_80186718[];

/// Renders `value` into `buf` as a fixed-point number with `decimals` digits
/// after the point, then appends the overlay's "%" suffix. The integer is
/// printed first (zero-padded to `decimals + 1` digits when it is too small to
/// fill them), then the last `decimals` characters are shifted one byte right
/// to open a slot for the '.'.

u8* func_mist_parking_8018182C(u8* buf, s32 value, s32 decimals)
{
    s32 remaining;
    s32 len;
    s32 shifted;
    s32 count;
    s32 scale;
    u8* p;

    scale     = 1;
    remaining = decimals;
    if (decimals > 0) {
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);
    }

    if (value < scale) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    count = decimals;
    p     = buf;
    len   = 0;
    if (count > 0) {
        if (*buf != 0) {
            do {
                p++;
                len++;
            } while (*p != 0);
        }
        if (len < count) {
            count = len;
        }
        count++;

        shifted = 0;
        if (count > 0) {
            do {
                p[1] = p[0];
                shifted++;
                p--;
            } while (shifted < count);
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_mist_parking_80186718);
    return buf;
}
