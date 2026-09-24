#include "common.h"

#include "rooms/room_common.h"

/// Hit-tests (`x`, `y`) against every rectangle of the `-1`-terminated hotspot
/// table, setting each entry's `hit` flag, and returns whether any was hit.
s32 func_neo_ark_shrine_8017EC10(RoomHotspot* table, s16 x, s16 y)
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
