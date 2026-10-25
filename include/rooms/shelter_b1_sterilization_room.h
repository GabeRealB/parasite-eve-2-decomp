#ifndef INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SterilizationRoomItemBoxModel;

/// Placement layouts indexed by this area's saved variant (0..10).
///
/// Variant 1 has empty, terminated placement and resource lists; variant 5
/// places the scene actor. Other slots contain two NULL pointers. Gameplay
/// borrows this directory and its mutable tables while the room is loaded.
extern AreaVariant gShelterB1SterilizationRoomAreaVariants[11];

/// Lighting descriptors for this area's three 1-based room IDs.
///
/// All three borrow the same mutable point/cone lights and the same 24-view
/// minimum-ambient table. The map overlay selects this array for the
/// sterilization-room area; its records and resources require this overlay
/// to remain loaded while lighting is queried or updated.
extern WorldCoordRoomLighting gShelterB1SterilizationRoomLighting[3];

/// Collision resources for this area's three 1-based room IDs.
///
/// All three borrow the same writable mesh, eight view boundaries, 28 action
/// triggers and two sight occluders. Trigger and occluder lists include their
/// LAST-marked final records. Keep this overlay loaded while the mesh is active
/// or its records are linked; room entry binds them to the current view.
extern WorldCollisionRoomResources gShelterB1SterilizationRoomCollisionResources[3];

/// Logical-to-mapped view maps for this area's three 1-based room IDs.
///
/// Each room uses 24 logical views, indexed at view - 1, with mapped indices
/// 1..24 shared by camera, image and sprite resources. Room 1 borrows gameplay's
/// identity map; rooms 2/3 borrow this overlay's mutable remaps. No terminator
/// is stored. The directory and room-owned maps live with this overlay.
extern u8* gShelterB1SterilizationRoomViewMaps[3];

/// Logical-view counts for the three 1-based room IDs, each with 24 views.
///
/// Indexed by room - 1 without a terminator. These signed-halfword counts
/// bound reverse searches of the room's logical-to-mapped view table.
extern ViewCount gShelterB1SterilizationRoomViewCounts[3];

/// Three warp endpoints, indexed by 1-based warp ID minus one.
///
/// Entries 1/2 connect to the Main Corridor and Pod Access Tunnel, with initial
/// logical views 2/8 and paired arrival/departure sounds. Entry 3 supplies a
/// silent placement in view 5. Player and companion share each entry's world
/// position and yaw (4096 units per turn). There is no terminator; the map
/// directory borrows the array while this overlay is loaded.
extern DirectionWarpEntry gShelterB1SterilizationRoomWarpEntries[3];

/// Camera transforms and pixel projection distances for mapped views 1..24.
///
/// Indexed by mapped view - 1, shared by all three room view maps, without a
/// terminator. Rotation is Q12 and translation is the negated world origin.
/// Queued camera requests borrow an entry until application, so keep it loaded.
extern ViewCamera gShelterB1SterilizationRoomViewCameras[24];

/// Sprite and image-background descriptors for mapped views 1..24.
///
/// Indexed by mapped view - 1 without a terminator. Views 8/18/20 select source
/// sprites; every view keeps decoded image strips and has no clipping list.
/// Batch lists end at SPRITE_BATCH_END. Descriptors and writable sources/lists
/// are borrowed from this overlay through packet allocation and drawing.
extern SpriteView gShelterB1SterilizationRoomSpriteViews[24];

/// Collision policies and footstep cues indexed by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Classes
/// 1/2 have distinct footstep cue sets; classes 0 and 3..7 share the record
/// without footstep sounds. The map overlay publishes this directory for the
/// sterilization-room area. Its borrowed records live with this overlay.
extern WorldCollisionSurfaceProperties* gShelterB1SterilizationRoomSurfacePropertiesByClass[8];

/// Initializes the sterilization room and updates its event actor each tick.
///
/// The map's room-task descriptor supplies a live task starting at state 0.
/// Dispatch states are 0 initialize, 1 update and 2 teardown; other indices are
/// invalid. Initialization registers `GAME_TASK_SLOT_ROOM`, installs the room's
/// message handlers, selects entry events and rebuilds the actor obstacle.
/// Spawn arguments are unused; the task and room resources live with the overlay.
void shelterB1SterilizationRoomTask(Task* task);

/// Draws view-dependent sterilization glows and emits smoke or trap puffs.
///
/// Gameplay effect 0x12F requires a live coordinate body. State 0 draws glows
/// and emits smoke in mapped views 20..24; mapped view 14 permanently enters
/// the trap-puff phase. A nonzero spawnArg1 arms one cancellation in view 6,
/// and the smoke views re-arm it. Other spawn arguments and Task::work are unused.
/// New particles require running room effects; glows continue while paused.
///
/// Puff sources are indices 0..63, grouped in four Q12 directions. Their packed
/// high halfword adjusts perspective size by -128, 96, 256 or 384 by view.
/// Smoke points occupy the room effect geometry's writable `smokePoints`;
/// both use the same random angle (4096 units/turn) and radius 256..767 game
/// units in the task coordinate's local space. Spawns copy their placement.
/// Requires loaded room resources, composed view transforms, a scratch stack
/// and the frame's GPU arena/table. It never retires itself; room teardown owns
/// its lifetime and releases the unused counted effect work through its exit callback.
void shelterB1SterilizationRoomEffectsTask(Task* task);

/// Animates one drifting, expanding sterilization puff through ten sprite frames.
///
/// Bank-6 effect 0x17D requires its counted, zeroed `EffectWork` allocation in
/// `spawnArg2.pointer` and a coordinate body. The low 12 bits of `spawnArg1`
/// must be a source index 0..63; each group of 16 selects one of four Q12
/// directions. The signed high halfword biases the initial perspective size.
/// `index` holds ticks per sprite frame (1..4), `period` the initial velocity
/// magnitude, and `step` the size growth per tick. Movement, size and age pause
/// while room effects are paused; drawing continues. Initialization while paused
/// leaves age zero, drawing signed frame -1 when the cadence is one tick.
/// Releases the work and task after the last frame.
void shelterB1SterilizationRoomPuffTask(Task* task);

/// Runs this room's telephone save menu and optional play-data statistics panels.
///
/// Gameplay selects this export for Mine/Shelter area 16. The live menu task
/// starts at state 0 and borrows a writable `UiObject` in `spawnArg2.pointer`
/// until the menu closes. Requires loaded telephone text/UI resources; save
/// and statistics dialogs are child tasks. Normal play before a clear enters
/// the save dialog directly; clear data or the statistics demo enables the list.
/// Completion reports the menu result through the object for its owner to close.
void shelterB1SterilizationRoomTelephoneMenuTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
