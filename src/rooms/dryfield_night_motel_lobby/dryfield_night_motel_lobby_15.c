#include "common.h"

#include <psyq/libgte.h>

#include "main/text.h"
#include "rooms/dryfield_night_motel_lobby.h"

/// Formats `value`, a percentage scaled by 10^`decimals`, into `buf` and
/// returns `buf`: the integer is printed padded to `decimals + 1` digits when
/// it is below that scale, a '.' is inserted before its last `decimals` digits,
/// and "%" is appended.
u8* func_dryfield_night_motel_lobby_8017EEA4(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_night_motel_lobby_80182508);
    return buf;
}
