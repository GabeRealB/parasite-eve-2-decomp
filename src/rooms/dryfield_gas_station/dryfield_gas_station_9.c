#include "common.h"

#include <psyq/libgte.h>

#include "main/text.h"

/// The "%" suffix appended to a formatted percentage.
extern u8 D_dryfield_gas_station_80181B78[];

/// Formats `value` into `buf` as a percentage with `decimals` fractional
/// digits: zero-padded to `decimals + 1` digits when smaller than
/// 10^`decimals`, with a '.' inserted before the last `decimals` digits and
/// "%" appended. Returns `buf`.
u8* func_dryfield_gas_station_8017EE54(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_gas_station_80181B78);
    return buf;
}
