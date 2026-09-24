#ifndef ROOMS_SHELTER_B2_SEPTIC_TANK_H
#define ROOMS_SHELTER_B2_SEPTIC_TANK_H

#include "common.h"
#include <psyq/libgte.h>

#include "main/task.h"

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z. A list of them ends at an entry whose `end` is -1; `end`
/// is not otherwise read.
typedef struct ShelterB2SepticTankSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 end;
} ShelterB2SepticTankSurface;

/// Per-surface values the water drawer keeps in a block taken from the
/// scratchpad stack at `0x1F8003FC` rather than in registers. `dx` and `dz`
/// are the spacing between vertices along X and Z, and `wave` the height the
/// sine wave adds to the vertex being placed.
typedef struct ShelterB2SepticTankWaterWork {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} ShelterB2SepticTankWaterWork;

/// The room's water surfaces, as two lists drawn by separate functions.
extern ShelterB2SepticTankSurface D_shelter_b2_septic_tank_801832CC[];
extern ShelterB2SepticTankSurface D_shelter_b2_septic_tank_801832F0[];

/// Height of the water surfaces.
extern s16 D_shelter_b2_septic_tank_801832BC;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b2_septic_tank_80187054;

void func_shelter_b2_septic_tank_8017DB68(Task* task);
void func_shelter_b2_septic_tank_8017E2DC(Task* task);
void func_shelter_b2_septic_tank_8017EAB8(Task* arg0);
void func_shelter_b2_septic_tank_8017EAF8(Task* task);
void func_shelter_b2_septic_tank_80180054(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_septic_tank_8018083C(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B2_SEPTIC_TANK_H
