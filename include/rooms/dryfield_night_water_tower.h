#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_water_tower_80182B5C[22];

/// Model-lighting resources for the night Water Tower's only room (room ID 1).
///
/// Stage 3 area 20 borrows this one-element room array while the overlay is loaded.
/// Its collection has four directional and five point lights, with no cone lights;
/// A NULL `ambientTable` selects the default ambient minima. Light transforms and
/// attenuation remain mutable during binding and shading.
extern WorldCoordRoomLighting gDryfieldNightWaterTowerRoomLighting[1];

/// Collision resources for the night Water Tower's only room (room ID 1).
///
/// Stage 3 area 20 borrows this room array. Its grid and both trigger arrays
/// are writable during binding and contact dispatch; there are no occluders.
/// Keep the overlay loaded until the grid is retired and triggers are unlinked.
extern WorldCollisionRoomResources gDryfieldNightWaterTowerCollisionRooms[1];

/// Room-1 mapping from logical views 1 through 10 to the same camera/image indices.
///
/// This one-element room directory borrows gameplay's 50-byte `gViewIdentityMap`.
/// The room view count limits reverse lookup independently of
/// the shared map's capacity. Keep this overlay and gameplay loaded during use.
extern u8* gDryfieldNightWaterTowerViewMaps[1];

/// Ten logical views searched through the identity map for night Water Tower room 1.
///
/// This signed halfword count limits reverse lookup; it is not the identity
/// map's capacity. The stage map borrows the one-element array while this
/// room overlay is loaded; there is no terminator.
extern ViewCount gDryfieldNightWaterTowerViewCounts[1];

/// Four night Water Tower warp endpoints, indexed by warp ID minus one.
///
/// IDs 1..4 serve the G & R kitchen, breezeway, water tank and motel room 6,
/// respectively. Entries supply world-coordinate spawns, yaw in 4096 units
/// per turn, the initial logical view, and departure/arrival effects. The
/// last two endpoints fade on departure. There is no terminator; the stage
/// directory borrows the array while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightWaterTowerWarpEntries[4];

/// Ten cameras indexed by the night Water Tower's mapped view index minus one.
///
/// The room's identity map selects entries 0..9 for logical views 1..10.
/// Rotations have 12 fractional bits, translations are negated world-space
/// camera origins in game units, and screen distances are pixels. There is no
/// terminator; a queued camera application borrows its entry until dispatch.
extern ViewCamera gDryfieldNightWaterTowerViewCameras[10];

/// Sprite resources for ten mapped views of the night Water Tower.
///
/// Indexed by mapped view minus one. Views 1 and 9 have empty source slots;
/// the others borrow source arrays and batches ending with `SPRITE_BATCH_END`.
/// Every first batch has zero count, retaining decoded background strips,
/// and every `drawAreas` pointer is NULL. Records and mutable source/batch arrays
/// remain borrowed while the room overlay is loaded.
extern SpriteView gDryfieldNightWaterTowerSpriteViews[10];

/// Collision and footstep policies indexed by room-local surface class 0..7.
///
/// Classes 0 and 2 block probes, allow weapon impacts and supply the same
/// footsteps. Class 1 passes probes, disables weapon impacts and has no footstep
/// cues. All apply grid pushback; classes 3..7 alias class 0. There is no
/// terminator. Stage 3 area 20 borrows the table and records while this overlay
/// is loaded; the room grid's authored faces use classes 0..2.
extern WorldCollisionSurfaceProperties* gDryfieldNightWaterTowerSurfacePropertiesByClass[8];

/// Draws the night Water Tower's light glows for the current room view.
///
/// Every tick enables the room's view-effect gate. Views 2 and 5 draw one
/// flare; view 3 draws a shaft and a flare; view 4 adds a second flare;
/// views 7 and 10 layer two identical flares at one point. Other views draw
/// nothing. `task` is unused; the callback keeps no per-task state.
/// Requires a live session and room effect controller, the loaded room overlay,
/// current view transforms and flare texture, initialized scratch space, and
/// room in the frame's packet arena and ordering table. Accepted light points
/// must have nonzero camera-Z / 4 depth. Packets remain live until GPU completion.
void dryfieldNightWaterTowerDrawGlowsTask(Task* task);

/// Runs the night water-tower room's registration, idle or teardown state.
///
/// `task->state` must be 0..2: 0 installs the message table and registers the
/// task in `GAME_TASK_SLOT_ROOM`, 1 keeps it alive for messages, and 2 kills it.
/// The room overlay must remain loaded until the task is torn down. Spawn
/// arguments and task work are unused.
void dryfieldNightWaterTowerRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
