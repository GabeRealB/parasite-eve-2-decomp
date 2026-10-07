#ifndef INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Storage for this room's shop session descriptor.
///
/// `desc` is the spawn recipe the shop menu hands to the stage transition when
/// the player opens the shop from this room: a bodyless task running the shop
/// session. Every other room carrying the shop defines the same descriptor as
/// a bare `TaskDesc` with the next object directly behind it, so the four
/// bytes that follow here are not part of the shop's own data. They are zero
/// in the image and have no established access; whether they are a separate
/// unreferenced variable or padding at an original object boundary is
/// unproven. They stay in this allocation only to keep the data that follows
/// at its address.
typedef struct {
    TaskDesc desc;         // Spawn recipe for the shop session task; read as a single descriptor
    u8       unknown_C[4]; // Zero in the image; no access established and role unproven
} ShelterB1UndergroundParkingShopSessionTaskDescStorage;
STATIC_ASSERT_SIZEOF(ShelterB1UndergroundParkingShopSessionTaskDescStorage, 16);

extern AreaVariant D_shelter_b1_underground_parking_8018B5C4[22];

extern ShelterB1UndergroundParkingShopSessionTaskDescStorage D_shelter_b1_underground_parking_801871F0;

// shelter_b1_underground_parking
extern WorldCoordRoomLighting D_shelter_b1_underground_parking_801877B4[];

extern WorldCollisionRoomResources D_shelter_b1_underground_parking_801877F4[];

extern u8* D_shelter_b1_underground_parking_8018791C[];

extern ViewCount D_shelter_b1_underground_parking_8018793C[];

extern DirectionWarpEntry D_shelter_b1_underground_parking_8018794C[];

extern ViewCamera D_shelter_b1_underground_parking_80189778[];

extern SpriteView D_shelter_b1_underground_parking_8018AB9C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_underground_parking_8018D724[];

/// Runs the underground parking room's initialization, interaction checks and shutdown.
///
/// The Shelter map's area-20 descriptor starts a bodyless task at state 0.
/// States 0, 1 and 2 initialize, check room interactions each frame and kill
/// the task, respectively; no state bounds check is performed. The room overlay
/// and its state table must remain loaded for the task's lifetime.
void shelterB1UndergroundParkingRoomTask(Task* task);

/// Updates this room's telephone save and play-data menu.
///
/// Gameplay routes the area-20 menu callback here. Requires a live UI object in
/// `task->spawnArg2.pointer` and this room overlay for the menu's lifetime.
/// State 0 opens saving directly during normal play; after a clear or in demo
/// scene 1 it offers saving and statistics. Child answers drive the notice and
/// reopening states, and cancellation is published on the owning UI object.
void shelterB1UndergroundParkingTelephoneMenuTask(Task* task);

/// Draws this room's light glows and beams for the current mapped view each frame.
///
/// Gameplay effect slot 0x133 calls this with an unused task argument. The
/// view mapping, view matrix, scratch stack, packet arena and depth ordering
/// table must be ready for the included glow drawers. Unlisted views emit no
/// packets. The cyan light uses the room's slow/fast pulse selection; the other
/// lights keep their frame-parity flicker. Queued packets borrow the current
/// frame arena until GPU completion; this callback owns no task work or body.
void shelterB1UndergroundParkingDrawGlowsTask(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
