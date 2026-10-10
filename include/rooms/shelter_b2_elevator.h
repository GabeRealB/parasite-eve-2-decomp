#ifndef INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H
#define INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Saved-area changes applied when the first elevator scene finishes or is skipped.
///
/// Selects layout 4 of the Shelter B4 upper sewer and sets its map mark in every
/// save mode. The second record is `AREA_APPLY_END`; the scene borrows the list
/// synchronously while this room overlay remains loaded.
extern AreaApplyRec gShelterB2ElevatorSceneSavedAreaUpdates[2];

extern AreaVariant D_shelter_b2_elevator_8017E964[11];

/// Logical-view mapping for the elevator's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..3 select the same camera,
/// image and sprite indices. The directory has no terminator and stays valid
/// only while the elevator overlay is loaded.
extern u8* gShelterB2ElevatorViewMaps[1];

/// Reverse-lookup view count for room 1: three logical views.
///
/// This signed element count is independent of the identity map's capacity
/// and the retained fifteen-camera table. There is no terminal count record.
extern ViewCount gShelterB2ElevatorViewCounts[1];

/// The elevator's single arrival and departure endpoint, selected by warp ID 1.
///
/// Places both actors facing a quarter turn at (11498, 0, -496) in view 2.
/// The loaded room owns the endpoint; direction handling copies it by value.
extern DirectionWarpEntry gShelterB2ElevatorWarps[1];

/// The elevator cab's six-face collision box and its single 4000-unit XZ cell.
///
/// Five faces use surface class 0 and the floor uses class 1. Activation binds
/// the view coordinate; keep the descriptor and its private mesh pools loaded
/// while collision queries use them.
extern WorldCollisionGrid gShelterB2ElevatorCollisionGrid;

/// Camera transforms and projection distances for the elevator's mapped views.
///
/// Views 1..3 select entries 0..2. Entries 3..14 retain copies of entry 2;
/// they do not extend the logical-view count. Queued camera tasks borrow these
/// records until dispatch, so this room overlay must remain loaded.
extern ViewCamera gShelterB2ElevatorViewCameras[15];

/// Sprite resources for mapped views 1..3, stored at index minus one.
///
/// View 2 adds 56 depth-sorted sprites to the decoded background. Views 1 and 3
/// use only that background. All batch lists include `SPRITE_BATCH_END`; none
/// supplies clipping areas. Keep the room and view textures loaded while used.
extern SpriteView gShelterB2ElevatorSpriteViews[3];

/// Authored elevator lighting: one point light contributing in every view.
///
/// Activation binds the light's coordinate and queries overwrite its cached
/// attenuation. Keep the descriptor and point-light storage loaded while bound.
extern WorldCoordRoomLights gShelterB2ElevatorRoomLights;

/// Directed view boundaries switching 3 to 2 and 2 to 3, in that order.
///
/// Both records are live; the second carries `WORLD_COLLISION_TRIGGER_LAST`.
/// Room setup binds and links them. Keep the storage loaded until unlinked.
extern WorldCollisionTrigger gShelterB2ElevatorViewBoundaryTriggers[2];

/// Facing-gated exit quad requesting the Shelter B2 elevator hall.
///
/// Uses departure warp 1 here and arrival warp 2 in the hall. The sole live
/// record carries `WORLD_COLLISION_TRIGGER_LAST`; keep it loaded until unlinked.
extern WorldCollisionTrigger gShelterB2ElevatorActionTriggers[1];

/// Collision-policy directory indexed by the elevator's surface class, 0..7.
///
/// Class 1 has its own record; every other class shares the default. Both block
/// probes, permit weapon impacts and apply pushback, with no footstep sounds.
/// The loaded room owns the directory and the two borrowed records.
extern WorldCollisionSurfaceProperties* gShelterB2ElevatorSurfacePropertiesByClass[8];

/// Door motion messages accepted by the active Shelter B2 elevator room task.
///
/// Sent after both leaf tasks have spawned; payload words are ignored. Motion
/// begins in their next update, clamps at each end and leaves the request active.
enum {
    SHELTER_B2_ELEVATOR_MESSAGE_OPEN_DOOR  = 5100,
    SHELTER_B2_ELEVATOR_MESSAGE_CLOSE_DOOR = 5101,
};

/// Maintains the Shelter B2 elevator's room controller and entry sequence.
///
/// Requires a live task with state 0..2: initialize the room and its door leaves,
/// idle while receiving room messages, or tear down with `taskKill`.
/// Dispatch is unchecked; keep this room overlay loaded for the task's lifetime.
void shelterB2ElevatorRoomTask(Task* task);

/// No-op callback for the Shelter B2 elevator's room-effect slot 0x139.
///
/// Leaves the task, its coordinate body and its state unchanged. Normal task
/// teardown owns their release; this room overlay must remain loaded meanwhile.
void shelterB2ElevatorEffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H
