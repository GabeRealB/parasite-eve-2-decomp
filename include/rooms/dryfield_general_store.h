#ifndef INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_general_store_80185654[13];

// dryfield_general_store
extern WorldCollisionRoomResources D_dryfield_general_store_8017E670[];

/// Logical-view map directory for the General Store's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1 through 16 select the same
/// 1-based camera, image and sprite indices. Keep the room overlay loaded while
/// borrowing this directory; the map itself belongs to gameplay.
extern u8* gDryfieldGeneralStoreRoomViewMaps[1];

extern ViewCount D_dryfield_general_store_8017E684[];

/// Lighting descriptor for the General Store's single room.
///
/// Room 1 borrows twelve mutable point lights and an ambient table for logical
/// views 1 through 16. All referenced storage lives in this room overlay and
/// must remain loaded through coordinate updates and shading queries.
extern WorldCoordRoomLighting gDryfieldGeneralStoreRoomLighting[1];

extern DirectionWarpEntry D_dryfield_general_store_8017E690[];

extern ViewCamera D_dryfield_general_store_8017F25C[];

extern SpriteView D_dryfield_general_store_8018402C[];

/// Collision policies and footstep cues indexed by room surface class 0 through 7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 1 supplies footsteps; classes 0 and 2 through 7 share a silent record.
/// The directory and borrowed records remain valid while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldGeneralStoreSurfacePropertiesByClass[8];

/// Inert callback for the General Store's room-effect slot 0xC4 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and neither advances nor kills the task.
/// Its coordinate body remains owned by the task until external teardown.
void dryfieldGeneralStoreNoOpEffectTask(Task* unusedTask);

/// Runs the General Store's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, broadcasts the pending entry actor
/// command or completes the started scene flag, and permits post-CAP sound
/// messages. Requires a live scene manager before the scene starts; keep the
/// store overlay loaded for the task's lifetime.
void dryfieldGeneralStoreRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
