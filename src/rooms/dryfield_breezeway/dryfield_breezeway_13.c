#include "common.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/room_common.h"

/// Hit-tests the point (`x`, `y`) against the 0xFFFF-terminated hotspot table
/// `table`, raising `hit` on every entry whose rectangle contains the point and
/// clearing it on every other one. Returns non-zero if any entry was hit.
s32 func_dryfield_breezeway_8017FCB4(RoomHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}
