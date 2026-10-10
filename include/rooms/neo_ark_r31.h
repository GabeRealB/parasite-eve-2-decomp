#ifndef INCLUDE_ROOMS_NEO_ARK_R31_H
#define INCLUDE_ROOMS_NEO_ARK_R31_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern s32 D_neo_ark_r31_8017DC54;

extern TaskDesc D_neo_ark_r31_8017D9E8;

extern AreaVariant D_neo_ark_r31_8017DBB8[13];

// neo_ark_r31
/// Logical-view map directory for the area's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..3 select the same
/// camera, image and sprite indices. Room IDs index at room minus one; there
/// is no terminator. The directory requires this room overlay to stay loaded.
extern u8* gNeoArkR31ViewMaps[1];

extern ViewCount D_neo_ark_r31_8017DA20[];

extern DirectionWarpEntry D_neo_ark_r31_8017DA24[];

extern ViewCamera D_neo_ark_r31_8017DA5C[];

/// Neo Ark r31's sprite resources, indexed by mapped view minus one.
///
/// All three views retain decoded backgrounds without source sprites.
/// Their distinct empty batch-list addresses also occupy sources.empty.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gNeoArkR31SpriteViews[3];

/// The room's point-light collection.
///
/// Contains no directional or cone lights. The room owns the mutable light
/// array; coordinate setup and queries require this overlay to remain loaded.
extern WorldCoordRoomLights gNeoArkR31RoomLights;

/// Neo Ark R31 collision and footstep policies for surface classes 0..7.
///
/// All eight classes share one record: probes block, weapon impacts are enabled,
/// pushback applies and footsteps are silent. Setup reads every entry; borrow
/// the directory and record only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkR31SurfacePropertiesByClass[8];

/// Runs the room controller and keeps RGB16 background masking armed.
///
/// Requires a live task with state 0 (register handlers and start the scene),
/// 1 (refresh the one-image decode mode), or 2 (kill). Keep the room, actor-461800
/// scene resources and Neo Ark map overlay loaded while the controller runs.
void neoArkR31RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R31_H
