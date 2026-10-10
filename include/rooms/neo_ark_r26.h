#ifndef INCLUDE_ROOMS_NEO_ARK_R26_H
#define INCLUDE_ROOMS_NEO_ARK_R26_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Placement layouts indexed directly by saved variant, 0..10.
///
/// Only default variant 1 contains actors: the two scripted walkers and their
/// resource recipes. All other slots are empty. Gameplay borrows this table
/// and its placement/resource lists while the room overlay is loaded.
extern AreaVariant gNeoArkR26AreaVariants[11];

/// Collision resources for room 1: one floor grid and no trigger or occluder lists.
///
/// The Neo Ark map directory indexes this array by room - 1. Grid activation
/// binds its mutable descriptor to the view; keep the room overlay loaded.
extern WorldCollisionRoomResources gNeoArkR26CollisionRooms[1];

/// Lighting for room 1: one point light and ambient minima for four views.
///
/// Indexed by room - 1 through the Neo Ark map directory. Light transforms and
/// attenuation remain writable; all referenced storage belongs to this overlay.
extern WorldCoordRoomLighting gNeoArkR26RoomLighting[1];

/// Room 1's logical-view map, borrowing the gameplay-owned identity map.
///
/// The directory is indexed by room - 1; logical views 1..4 select matching
/// camera, image and sprite indices. Keep both gameplay and this overlay loaded.
extern u8* gNeoArkR26ViewMaps[1];

/// Reverse-lookup limit for room 1's four logical views, indexed by room - 1.
extern ViewCount gNeoArkR26ViewCounts[1];

/// Arrival warp 1, placing player and companion at the origin facing a half turn.
///
/// Starts in view 2 with no sound, scripted-player option or map-marker update.
/// The Neo Ark warp directory indexes by warp - 1 and borrows this overlay.
extern DirectionWarpEntry gNeoArkR26Warps[1];

/// Cameras for mapped views 1..4, stored at mapped view - 1.
///
/// View 1 looks straight down; the entry scene uses views 2..4. Transforms use
/// world-coordinate units and 4096 per unit rotation; projection distances
/// are pixels. Keep the overlay loaded until a queued camera has been applied.
extern ViewCamera gNeoArkR26ViewCameras[4];

/// Sprite resources for mapped views 1..4, stored at mapped view - 1.
///
/// Views 1 and 4 have no sprites; views 2 and 3 supply 62 and 14 depth-sorted
/// rectangles. Every batch list retains decoded background strips and ends
/// with SPRITE_BATCH_END. Source and batch storage belongs to this overlay.
extern SpriteView gNeoArkR26SpriteViews[4];

/// Collision policies indexed by room-local surface class, 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Class 1, used by the floor grid, adds paired footstep cues; the other
/// classes have no footstep record. All pointers borrow this room overlay.
extern WorldCollisionSurfaceProperties* gNeoArkR26SurfaceProperties[8];

/// Leaves the room's effect task unchanged each tick.
///
/// Effect bank 6, slot 0x168 supplies a single-coordinate task. This callback
/// ignores it, draws nothing and performs no teardown; the task remains live
/// until another owner tears it down. The `neo_ark_r26` overlay must remain
/// loaded while this callback can be dispatched.
void neoArkR26EffectNoopTask(Task* unusedTask);

/// Runs the Neo Ark room 26 room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and the entry scene, remain idle, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkR26RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R26_H
