#include "common.h"

#include <psyq/libgte.h>

#include "main/text.h"
#include "rooms/shelter_b1_sterilization_room.h"

/// Formats `value` into `buf` as a percentage with `decimals` digits after the
/// point, zero-padding a small value so that a digit precedes the point, then
/// appends "%" and returns `buf`.
u8* func_shelter_b1_sterilization_room_8017EEF0(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_shelter_b1_sterilization_room_80184594);
    return buf;
}
