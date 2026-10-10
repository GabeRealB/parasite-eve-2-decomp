#ifndef SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"

#include "main/task_types.h"

extern AnimationSet gDryfieldNightGarageAnimation04B80;

extern AnimationSet gDryfieldNightGarageAnimation04F54;

extern AnimationSet gDryfieldNightGarageAnimation05344;

extern AnimationSet gDryfieldNightGarageAnimation056B0;

extern TaskDesc D_dryfield_night_garage_80182C98[2];

extern s32 D_dryfield_night_garage_80182DE0;

extern s32 D_dryfield_night_garage_80182DE4;

extern EvsCommand D_dryfield_night_garage_80182DF8[40];

extern EvsCommand D_dryfield_night_garage_801831B8[19];

/// Room 1's writable collision mesh and XZ cell index for the night garage.
///
/// Contains 55 faces, 40 normals, 110 vertices and nine X-major cells. Cell
/// coordinates use (X + 150) / 4000 and Z / 4000 in game-coordinate units.
/// The first four faces/normals and eight vertices are reserved for the low or
/// tall collision box; room scripts replace them without rebuilding cell lists.
/// Activation borrows the current view coordinate. The descriptor and pools
/// must remain live while active or edited, including edits made while room 2
/// selects its separate grid.
extern WorldCollisionGrid gDryfieldNightGarageEarlyRoomCollisionGrid;

/// Room 1's sixteen interaction and warp triggers, including the final live record.
///
/// The list ends with WORLD_COLLISION_TRIGGER_LAST on entry 15. Entries 3 and
/// 5 select the post-refueling and original refueling hotspots; room progress
/// switches their enabled bits. Other entries select dialogue, companion
/// interaction, CAP commands and the factory/junk-yard exits. Setup writes
/// links, coordinates and enabled bits, so keep this room-owned storage loaded
/// until unlinked. Positions and radii are game units; normals use 4096 per unit.
extern WorldCollisionTrigger gDryfieldNightGarageEarlyRoomActionTriggers[16];

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalogue row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalogue must remain loaded throughout the session.
extern const EquipmentWeaponSupply* gShopRefillSupply;

/// Finds a placed actor in the current stage and area, or returns NULL.
///
/// `placementIndex` must be 0..15; it occupies the high nibble of the u16
/// placement key. Requires a live scene manager whose children carry Enemy
/// work. Returns a borrowed task, valid only while that actor remains live.
Task* dryfieldNightGarageFindPlacedActor(s32 placementIndex);

/// Runs Gary Douglas's dialogue, shop session and progress-dependent reply.
///
/// Starts in state 0 with player control held; `spawnArg1.value` supplies the
/// signed low-halfword CAP sequence slot (10 or 21 from the room action).
/// Turns the placement-0 actor's head toward the player during playback,
/// opens the default Dryfield stock, then plays response repeat-flag + 1.
/// Completion resumes player control and releases the head turn. Requires the
/// actor, room CAP resources and shop descriptors to remain live throughout.
void dryfieldNightGarageShopDialogueTask(Task* task);

/// Stages audio start for the refueling scene selected by the event script.
///
/// View loading later commits the deferred request. Selection and
/// playback buffers must remain live until it is consumed.
void dryfieldNightGarageStageSceneAudioStart(void);

/// Enqueues playback of the refueling scene after its setup and caption cues.
///
/// Requires the selected scene's prepared buffers to survive playback and
/// space in the CD request ring.
void dryfieldNightGarageEnqueueScenePlayback(void);

/// Finishes refueling-scene streaming and restores the saved random state.
///
/// Requires a successful scene selection. Marks streaming complete without
/// freeing buffers or tasks; the event script restores the remaining resources.
void dryfieldNightGarageFinishScene(void);

/// Cancels the refueling scene when its skip script restores the room.
///
/// Discards the deferred CD replacement and requests cancellation; subsequent
/// CD dispatches complete it. Finishes streaming and restores saved random
/// state immediately. Requires a successful scene selection; the script owns
/// buffer and task cleanup.
void dryfieldNightGarageCancelScene(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
