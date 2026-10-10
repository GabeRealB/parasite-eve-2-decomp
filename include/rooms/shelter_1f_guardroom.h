#ifndef INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
#define INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources for the guardroom's single room, indexed by room ID minus one.
///
/// Borrows the mutable grid and terminated trigger lists while this overlay is
/// loaded. There are no sight occluders; unlink the triggers before unloading.
extern WorldCollisionRoomResources gShelter1fGuardroomRoomCollisionResources[1];

/// Point lights and per-view ambient minima for the guardroom's single room.
///
/// Indexed by room ID minus one. Light transforms remain writable and all
/// referenced storage is borrowed only while the guardroom overlay is loaded.
extern WorldCoordRoomLighting gShelter1fGuardroomRoomLighting[1];

/// Logical-view maps for the guardroom's single room, indexed by room ID minus one.
///
/// Borrows `gViewIdentityMap`: views 1..3 select camera, image and sprite slots
/// 1..3. The gameplay map has greater capacity than this room's view count.
extern u8* gShelter1fGuardroomViewMaps[1];

/// Logical-view search counts for the guardroom's single room.
///
/// Indexed by room ID minus one; its three views use the identity mapping.
extern ViewCount gShelter1fGuardroomRoomViewCounts[1];

/// The guardroom's arrival endpoint from the vehicular airlock.
///
/// Warp ID 1 selects index 0, placing the player and companion at the doorway
/// in view 2. Supplies arrival and departure sounds, with no blocked sound or
/// map marker. The map directory borrows it while this overlay is loaded.
extern DirectionWarpEntry gShelter1fGuardroomWarpEntries[1];

/// Cameras for guardroom views 1..3, indexed by mapped view minus one.
///
/// View 1 looks down at the room; views 2 and 3 cover the doorway and bulwark
/// control. Rotations use `ONE` per unit, translations negate world-space camera
/// origins, and projection distances are pixels. Borrow only while loaded.
extern ViewCamera gShelter1fGuardroomViewCameras[3];

/// Sprite resources for guardroom views 1..3, indexed by mapped view minus one.
///
/// All views retain the decoded background. Only view 3 has source sprites:
/// batch 1 contains the two-sprite unlocked-bulwark overlay and its mutable
/// visibility flag. No view clips drawing; borrow the tables only while loaded.
extern SpriteView gShelter1fGuardroomSpriteViews[3];

/// Guardroom collision policies indexed by surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 1 is the grid's floor and has footstep cues; class 0 covers walls and
/// ceiling. Classes 2..7 retain the same silent policy as class 0. Records are
/// borrowed only while this overlay is loaded; the table has no terminator.
extern WorldCollisionSurfaceProperties* gShelter1fGuardroomSurfaceProperties[8];

/// Runs the Shelter 1F Guardroom room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and the unlock overlay, remain idle, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void shelter1fGuardroomRoomTask(Task* task);

/// Room-effect callback that leaves the guardroom's effect task idle.
///
/// Effect bank 6, slot 0x167 supplies a single-coordinate task. This callback
/// ignores the task, draws nothing and performs no teardown. The guardroom
/// overlay must remain loaded while the callback can be dispatched.
void shelter1fGuardroomEffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
