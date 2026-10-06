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

void func_mist_parking_80183BAC(s32 arg0);

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
