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

/// Updates the parking-lot telephone's save menu and optional play statistics.
///
/// `spawnArg2.pointer` must hold the live UI object owned by this menu task.
/// Its save and statistics children must remain linked until their answers
/// are consumed. Requires the parking overlay and telephone UI resources.
void mistParkingTelephoneMenuTask(Task* task);

/// Starts the selected parking departure script once EVS and CAP are idle.
///
/// The room answer word selects departure when it is 2, staying otherwise.
/// The producer of that word is unproven. Kills this continuation after starting
/// the script; the callback does not use its work, body or spawn arguments.
/// Requires the parking overlay and its dialogue resources to remain loaded.
void mistParkingContinueDepartureChoiceTask(Task* task);

/// Draws the current mapped camera view's grey capsule glows and pulsing cyan point glow.
///
/// Per-frame effect callback for bank 6, slot 0x39; `unused` is ignored.
/// Requires the room overlay and view map to remain loaded, the current view
/// matrix to be composed, and an initialized scratch stack, ordering table and
/// packet arena. Queued additive packets live until that frame's GPU work ends.
/// Views without a listed glow emit no packets; task state is unchanged.
void mistParkingDrawGlowsTask(Task* unused);

/// Runs the parking-lot room's message receiver and arrival conversation setup.
///
/// `state` is 0 to initialize, 1 to idle or 2 to release, with no bounds check.
/// Initialization registers the task in the room slot and may start the
/// variant-2 arrival conversation. Requires the live session, save state and
/// parking overlay to stay loaded until the task is released.
void mistParkingRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MIST_PARKING_H
