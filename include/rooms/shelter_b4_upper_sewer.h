#ifndef ROOMS_SHELTER_B4_UPPER_SEWER_H
#define ROOMS_SHELTER_B4_UPPER_SEWER_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z, cut into `count` flat quads. The quads are laid along X
/// when `alongZ` is zero and along Z otherwise. A list of them ends at an entry
/// whose `count` is -1.
typedef struct ShelterB4UpperSewerSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 count;
    s16 alongZ;
} ShelterB4UpperSewerSurface;

/// Per-surface values the water drawer keeps in a block taken from the
/// scratchpad stack at `0x1F8003FC` rather than in registers. `dx` and `dz`
/// are the spacing between vertices along X and Z, and `wave` a height added
/// to the far edge of each quad, always zero here. The layout is the one the
/// water drawers of `shelter_b4_water_supply` use; whether the two are one
/// type is open.
typedef struct ShelterB4UpperSewerWaterWork {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} ShelterB4UpperSewerWaterWork;

/// State handlers of the task `func_shelter_b4_upper_sewer_8017DC30` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup (message table, pointer slot, water level), an idle
/// state, and `taskKill`.
extern const TaskFuncTable3 D_shelter_b4_upper_sewer_8017D5C4;

void func_shelter_b4_upper_sewer_8017DC30(Task* task);
void func_shelter_b4_upper_sewer_8017EA0C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_upper_sewer_8017F8CC(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_upper_sewer_80180AC4(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b4_upper_sewer_8018139C(GsCOORDINATE2* coord, s16 size);
void func_shelter_b4_upper_sewer_80181C40(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b4_upper_sewer_80182E04(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b4_upper_sewer_80183D08(GsCOORDINATE2* coord, s16 frame, u8* rgb);

#endif // ROOMS_SHELTER_B4_UPPER_SEWER_H
