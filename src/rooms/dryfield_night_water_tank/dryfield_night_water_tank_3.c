#include "common.h"

#include "gameplay/268.h"
#include "main/session.h"
#include "rooms/dryfield_night_water_tank.h"

/// Restores the room's layout lists from their template, then offsets the six
/// `field_8` coordinates by (0, 0, -0xC8) when `arg0` is non-zero.
void func_dryfield_night_water_tank_8017D9DC(s32 arg0)
{
    DryfieldNightWaterTankLayout* dst;
    DryfieldNightWaterTankLayout* src;
    DryfieldNightWaterTankVec     d;
    s32                           i;

    dst = &D_dryfield_night_water_tank_8017F4B0;
    src = &D_dryfield_night_water_tank_8017E08C;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
        d.z = 0;
    } else {
        d.x = 0;
        d.y = 0;
        d.z = -0xC8;
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_3", func_dryfield_night_water_tank_8017DB8C);

void func_dryfield_night_water_tank_8017DD8C(void)
{
}
