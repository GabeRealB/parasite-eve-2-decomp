#ifndef INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models the Neo Ark map UI overlay's enemy descriptors attach.
extern TmdSource gShelter1fVehicularAirlockModel03A58;

extern AreaVariant D_shelter_1f_vehicular_airlock_80182A04[12];

// shelter_1f_vehicular_airlock
extern WorldCollisionRoomResources D_shelter_1f_vehicular_airlock_801820FC[];

extern WorldCoordRoomLighting D_shelter_1f_vehicular_airlock_8018210C[];

extern u8* D_shelter_1f_vehicular_airlock_80182114[];

extern ViewCount D_shelter_1f_vehicular_airlock_80182118[];

extern DirectionWarpEntry D_shelter_1f_vehicular_airlock_8018211C[];

extern ViewCamera D_shelter_1f_vehicular_airlock_8018245C[];

extern SpriteView D_shelter_1f_vehicular_airlock_801824F8[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_vehicular_airlock_80182A80[];

/// Hides the placed airlock model while its saved two-bit object state is 2.
///
/// Requires a live TMD body and the owning `Enemy` in `spawnArg2.pointer`, as
/// supplied by area object placement. The low byte of its place key selects
/// object-state flag 0..63 (the map places this model at flag 6). State 2 sets
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW`; every other state clears it. Other flags and
/// task state are preserved. Both the body and session flag bank must remain live.
void shelter1fVehicularAirlockUpdatePlacedModelVisibilityTask(Task* task);

void func_shelter_1f_vehicular_airlock_8017DA48(Task* task);

/// Runs the vehicular airlock's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, as installed by `effectSpawn`. `spawnArg1.value` must
/// start as a positive charge duration in active ticks; charging consumes it.
/// Nonzero room effect control pauses the flash; control 4 or above, state 3,
/// or completion releases the effect work and task.
void shelter1fVehicularAirlockRoomVisualEffectsFlashTask(Task* task);

/// Draws fading twin trails from two fixed offsets on the effect's parent.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its parent coordinate must stay live until teardown.
/// `Task::work` starts null and owns two allocated eight-coordinate histories.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish;
/// zero leaves lifetime to external teardown. Age includes initialization and
/// advances only with room effect control below 2. Allocation failure resets
/// age for retry. Teardown releases both the histories and the effect work.
void shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_1f_vehicular_airlock_80180008(Task* task);

/// Draws the vehicular airlock's fixed light glows in mapped views 2 and 3.
///
/// Bank-6 task 0x164 installs this room's flash, twin-trail and spark-burst IDs
/// on its first tick. View 3 uses pulsing stars for three lamps while the
/// bulwark-unlocked flag is 1, and flickering discs otherwise. Other views emit
/// no geometry. Requires this overlay loaded, current view matrices, an
/// initialized scratch stack, and frame packet/ordering-table space. Queued
/// packets borrow the frame arena until GPU completion; the task owns no work.
void shelter1fVehicularAirlockDrawLightsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
