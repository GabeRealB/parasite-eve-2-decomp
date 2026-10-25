#ifndef INCLUDE_ROOMS_NEO_ARK_R31_H
#define INCLUDE_ROOMS_NEO_ARK_R31_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Horizontal framebuffer displacement in pixels for the scene distortion.
///
/// Nonnegative values shift the captured image left; any negative value requests
/// the redraw task's exit. The task starts at 3 and the scene actor publishes
/// its 3..8-pixel ramp. Requires the R31 room overlay to remain loaded.
extern s32 gNeoArkR31FramebufferShiftPixels;

/// Bodyless shifted-framebuffer overlay task recipe started on room entry.
///
/// One descriptor, priority `TASK_PRIORITY_CONTROL`, zero metadata, with no terminator. Spawn at
/// index 0 with zero arguments; the callback borrows the loaded room overlay.
extern TaskDesc gNeoArkR31FramebufferShiftTaskDesc;

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

/// Neo Ark R31 logical-view search count for its single room.
///
/// Room 1 searches three identity-map entries, selecting mapped views 1..3.
/// Indexed by room - 1; signed halfword values count logical map elements,
/// independently of map capacity and mapped resource extents. No terminator.
/// Borrow the array only while this room overlay is loaded.
extern ViewCount gNeoArkR31ViewCounts[1];

/// A single warp endpoint record, selected by warp ID 1.
///
/// The single endpoint places both actors at the world origin, facing yaw 2048, in view
/// 1. It has no sounds, transition flags or map marker.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gNeoArkR31WarpEntries[1];

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
