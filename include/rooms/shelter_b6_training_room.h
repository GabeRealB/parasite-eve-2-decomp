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

/// Draws the paired enemy's yellow body glow and publishes its energy-arc anchor.
///
/// Requires a live TMD body with at least 19 coordinates and composed joint 1.
/// While effects run, borrows joint 1 as the arc endpoint, draws two discs,
/// and has a one-in-four chance to spawn a flash at a joint in 3..18.
/// The model must outlive every arc borrowing that endpoint.
void shelterB6TrainingRoomDrawBodyGlow(Task* task);

/// Occasionally spawns a joint-attached energy arc for the shielded enemy.
///
/// While effects run, has a one-in-eight chance per call to select joint
/// 1..18. Requires a TMD body with at least 19 coordinates, a live body-glow
/// anchor, and model coordinates that outlive the spawned arc tasks.
void shelterB6TrainingRoomSpawnShieldArcs(Task* task);

/// Shows or hides the destroyed-part sprites for one training-room enemy part.
///
/// `partSlot` selects 0 or 1; `destroyed` is 0 for intact, 1 for destroyed.
/// Other byte values do nothing. Updates that part's sprite batches in views
/// 2 and 6 of the current stage and area, whose sprite tables must be loaded.
void shelterB6TrainingRoomSetPartDestroyedSprites(u8 partSlot, u8 destroyed);

/// Draws a two-layer additive beam from the live summon-ring origin to an endpoint.
///
/// Borrows composed translations; does nothing before an origin is published
/// or if either projection has negative GTE flags. `colorIndex` is 0..3 in
/// the four-step RGB-nibble ramp. `radiusScale` is signed: each layer's pixel
/// radius is radiusScale * layer * 64 / (SZ3 / 4), for layers 1 and 2.
/// Accepted depths must be nonzero. Each layer also draws a ground glow,
/// using the selected packed RGB value as its second palette lookup's offset;
/// that lookup's backing-storage bounds remain unproven.
/// Requires scratch-stack space and an unchecked GPU frame arena. The origin
/// and endpoint must remain live through the call; queued packets live for
/// the frame. Overwrites GTE transform and projection registers.
void shelterB6TrainingRoomDrawSummonBeam(const GfxCoord* endCoord, s16 radiusScale, u16 colorIndex);

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

/// Builds the orange charge glow, then releases three ring bands and a fading screen tint.
///
/// `spawnArg1.value` starts as a positive countdown in running effect ticks.
/// Initialization resets rotation and starts the brightness ramp; expiry
/// emits bands 0..2. Held effects redraw without advancing countdown or fade,
/// although age advances. Cancellation or the completed fade releases the
/// task's owned `EffectWork` and coordinate body. Other task states draw
/// nothing and retain the effect until room-effect cancellation.
void shelterB6TrainingRoomChargeBurstTask(Task* task);

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

/// Moves a four-frame additive sprite down local Y and occasionally sheds a sinking sprite.
///
/// Adds 32 coordinate units per running tick, cycles texture cells and draws
/// on odd ages, then releases its owned `EffectWork` at age 60. Drawing uses
/// the cached composed translation before this tick's movement is composed.
/// Outside running effect control it retains its work and draws nothing,
/// including cancellation.
void shelterB6TrainingRoomDescendingSpriteTask(Task* task);

/// Emits descending sprites along the room's rising heal spiral for 21 running ticks.
///
/// The fixed centre has a 1000-unit XZ radius; successive spawn positions
/// rise 200 coordinate units and advance a random 512..1023 angle units
/// (4096 per turn). Holds outside running effect control. Releases the owned
/// `EffectWork` on the callback after the 21st emission, even while held.
void shelterB6TrainingRoomHealSpiralTask(Task* task);

/// Draws the fixed room glows visible in the mapped camera view.
///
/// On its first tick, resets the hit-flash generation and seeds the selected
/// band's six texture phases with three complete passes of the LCG.
/// `spawnArg1.value` selects band 0..2; no effect work block is required.
void shelterB6TrainingRoomGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H
