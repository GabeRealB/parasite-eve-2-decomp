#ifndef INCLUDE_ROOMS_MIST_PARKING_H
#define INCLUDE_ROOMS_MIST_PARKING_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_mist_parking_8018668C;

extern AreaVariant D_mist_parking_801951B4[13];

// mist_parking
extern WorldCollisionRoomResources D_mist_parking_8019155C[];

extern u8* D_mist_parking_801915B0[];

extern ViewCount D_mist_parking_801915C0[];

extern WorldCoordRoomLighting D_mist_parking_801915C8[];

extern DirectionWarpEntry D_mist_parking_801915E8[];

extern ViewCamera D_mist_parking_80192228[];

extern SpriteView D_mist_parking_8019399C[];

extern WorldCollisionSurfaceProperties* D_mist_parking_801952F0[];

/// Placement choices for the collision patch controlled with Pierce's visibility.
enum {
    MIST_PARKING_PIERCE_PATCH_RESTORED = 0,
    MIST_PARKING_PIERCE_PATCH_LOWERED  = 1
};

/// Restores Pierce's collision patch, optionally lowering it by 2000 world Y units.
///
/// Zero restores its original position; every nonzero `lowerPatch` applies the
/// positive Y offset. Re-copies two normals/faces and six XYZ vertices into the
/// live room grid before translating, so repeated calls never accumulate offsets.
/// Vector fourth halfwords, the rest of the grid and its cell table are untouched.
/// Requires the mist_parking overlay and its writable live grid to remain loaded.
void mistParkingSetPierceCollisionPatchLowered(s32 lowerPatch);

void func_mist_parking_80181468(Task* task);

/// Draws the current mapped camera view's grey capsule glows and pulsing cyan point glow.
///
/// Per-frame effect callback for bank 6, slot 0x39; `unused` is ignored.
/// Requires the room overlay and view map to remain loaded, the current view
/// matrix to be composed, and an initialized scratch stack, ordering table and
/// packet arena. Queued additive packets live until that frame's GPU work ends.
/// Views without a listed glow emit no packets; task state is unchanged.
void mistParkingDrawGlowsTask(Task* unused);

void func_mist_parking_80182898(Task* task);

#endif // INCLUDE_ROOMS_MIST_PARKING_H
