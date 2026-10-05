#ifndef INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"

extern AreaVariant D_shelter_b6_training_room_801859DC[13];

// shelter_b6_training_room
extern u8* D_shelter_b6_training_room_80184418[];

extern ViewCount D_shelter_b6_training_room_8018441C[];

extern DirectionWarpEntry D_shelter_b6_training_room_80184420[];

extern WorldCollisionGrid D_shelter_b6_training_room_80184734;

extern ViewCamera D_shelter_b6_training_room_80184758[];

extern SpriteView D_shelter_b6_training_room_80184D78[];

extern WorldCoordRoomLights D_shelter_b6_training_room_80185768;

extern WorldCollisionTrigger D_shelter_b6_training_room_80185780[];

extern WorldCollisionTrigger D_shelter_b6_training_room_80185A44[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_training_room_80185BC0[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_training_room_80185C38[];

void func_shelter_b6_training_room_8017D8E8(Task* task);

void func_shelter_b6_training_room_80181930(Task* task);

void func_shelter_b6_training_room_8018294C(Task* task);

/// Shows or hides the destroyed-part sprites for one training-room enemy part.
///
/// `partSlot` selects 0 or 1; `destroyed` is 0 for intact, 1 for destroyed.
/// Other byte values do nothing. Updates that part's sprite batches in views
/// 2 and 6 of the current stage and area, whose sprite tables must be loaded.
void shelterB6TrainingRoomSetPartDestroyedSprites(u8 partSlot, u8 destroyed);

void func_shelter_b6_training_room_8017FC40(GfxCoord* coord, s16 size, u16 color);

/// Runs the orange disc, halo burst and expanding outer-band effect.
///
/// Uses the task's owned `EffectWork` and composed coordinate. Pauses while
/// room effects are held and releases on cancellation or after fading.
void shelterB6TrainingRoomOrangeBurstTask(Task* task);

/// Builds and fades the violet summon ring, retaining its beam origin.
///
/// `spawnArg1.value` starts as a positive countdown in running effect ticks.
/// After buildup the coordinate becomes the room's beam origin; the work
/// block stays live in state 3 until the actor requests state 4 for release.
/// Held effects redraw with their countdown and brightness unchanged; age
/// still advances. Cancellation releases the effect.
void shelterB6TrainingRoomSummonRingTask(Task* task);

void func_shelter_b6_training_room_80180DB4(Task* task);

/// Expands and fades one of the three textured ring bands.
///
/// `spawnArg1.value` selects band 0..2. Band 0 rises before fading; bands 1
/// and 2 fade immediately with different expansion rates. Initialization
/// flattens local Y to zero. Held effects redraw; cancellation draws once
/// more before releasing the task's owned `EffectWork`.
void shelterB6TrainingRoomRingBandTask(Task* task);

/// Animates a joint-attached sprite and its strip to the body-glow anchor.
///
/// Reparents the task coordinate to the `EffectWork` parent at its saved
/// offset. Chooses a lifetime threshold of 6..21 running ticks, drawing the
/// strip on odd ages and releasing after the age exceeds that threshold.
/// Requires a live body-glow anchor with a composed matrix. Outside running
/// effect control it keeps its work and draws nothing, including cancellation.
void shelterB6TrainingRoomEnergyArcTask(Task* task);

/// Runs the yellow hit flash, replacing earlier flashes with the newest one.
///
/// Stamps `spawnArg1.value` with the shared 16-bit generation on first use.
/// Holds while effects are paused; releases when superseded, cancelled or
/// faded. Brightness starts fading on the ninth running tick.
void shelterB6TrainingRoomHitFlashTask(Task* task);

/// Moves a palette-1 animated sprite down the local Y axis.
///
/// Adds eight coordinate units each running tick and displays eight texture
/// cells on alternate ticks before releasing the task's owned `EffectWork`.
/// Chooses a random screen angle in 4096 units per turn. Outside running
/// effect control it keeps its work and draws nothing, including cancellation.
void shelterB6TrainingRoomSinkingSpriteTask(Task* task);

void func_shelter_b6_training_room_801826E0(Task* task);

void func_shelter_b6_training_room_80182804(Task* task);

/// Draws the fixed room glows visible in the mapped camera view.
///
/// On its first tick, resets the hit-flash generation and seeds the selected
/// band's six texture phases with three complete passes of the LCG.
/// `spawnArg1.value` selects band 0..2; no effect work block is required.
void shelterB6TrainingRoomGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H
