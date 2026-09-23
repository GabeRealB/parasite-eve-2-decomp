#include "common.h"

#include "gameplay/3A34.h"
#include "main/gameflag.h"

/// The template the room's collision grid is restored from, and the grid
/// itself.
extern GpGridParams D_dryfield_toilet_80180314;
extern GpGridParams D_dryfield_toilet_80181404;

/// Restores one face of the room's collision grid (its normal, four corners and
/// face record) from the template, then slides the four corners 2000 units toward
/// negative x once game flag nibble 0x60 is set.
void func_dryfield_toilet_8017D5E4(void)
{
    GpGridParams* geom = &D_dryfield_toilet_80181404;
    GpGridParams* src  = &D_dryfield_toilet_80180314;
    s32           i;

    for (i = 0; i < 1; i++) {
        geom->field_4[i].vx         = src->field_4[i].vx;
        geom->field_4[i].vy         = src->field_4[i].vy;
        geom->field_4[i].vz         = src->field_4[i].vz;
        geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
        geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
        geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
        geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
        geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
        geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
        geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
        geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
        geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
        geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
        geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
        geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
        geom->field_C[i]            = src->field_C[i];
    }
    if (GameFlag_GetNibble(0x60) != 0) {
        for (i = 0; i < 4; i++) {
            geom->field_8[i].vx -= 2000;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_toilet/dryfield_toilet", RoomsShared8017d878Table);
