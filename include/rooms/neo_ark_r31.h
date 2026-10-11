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

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_R31_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkR31AreaVariants[NEO_ARK_R31_AREA_VARIANT_COUNT];

// neo_ark_r31
/// Logical-view map directory for the area's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..3 select the same
/// camera, image and sprite indices. Room IDs index at room minus one; there
/// is no terminator. The directory requires this room overlay to stay loaded.
extern u8* gNeoArkR31ViewMaps[1];

extern ViewCount D_neo_ark_r31_8017DA20[];

extern DirectionWarpEntry D_neo_ark_r31_8017DA24[];

/// Authored cameras for mapped view indices 1..3, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gNeoArkR31ViewCameras[3];

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
