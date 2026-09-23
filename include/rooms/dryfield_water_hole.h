#ifndef ROOMS_DRYFIELD_WATER_HOLE_H
#define ROOMS_DRYFIELD_WATER_HOLE_H

#include "common.h"

/// One rectangle of water surface drawn by `func_dryfield_water_hole_8017D898`,
/// in world coordinates: it spans `width` along X from `x` and `depth` along Z
/// from `z`, at height `y`. The table ends at the first entry whose `y` word is
/// -1; the drawing code reads only its low half as the height.
typedef struct DryfieldWaterHoleSurface {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} DryfieldWaterHoleSurface;

/// The room's water surfaces, terminated by an entry with `y == -1`.
extern DryfieldWaterHoleSurface D_dryfield_water_hole_8017FC98[];

/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_water_hole_801828CC;

/// Frame counter the water surface's wave is phased by.
extern s16 D_dryfield_water_hole_801828D0;

/// Draws the room's water surfaces into the current ordering table.
void func_dryfield_water_hole_8017D898(void);

#endif
