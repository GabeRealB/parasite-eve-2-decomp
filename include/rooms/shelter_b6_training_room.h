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

/// Number of mapped camera and sprite views in the training room.
enum { SHELTER_B6_TRAINING_ROOM_VIEW_COUNT = 8 };

/// Logical-view map for the training room's single room slot.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..8 select the same
/// 1-based camera, image and sprite indices. The directory lives with this
/// room overlay; the shared byte map lives with gameplay.
extern u8* gShelterB6TrainingRoomViewMaps[1];

extern ViewCount D_shelter_b6_training_room_8018441C[];

extern DirectionWarpEntry D_shelter_b6_training_room_80184420[];

/// The training room's wall, floor and raised-platform collision grid.
///
/// Six X-major cells (2 by 3), each 4000 game-coordinate units wide, cover
/// X from 0 and Z from 750. Borrows writable pools of 7 normals, 22 vertices
/// and 21 faces. Activation binds the view coordinate; the room overlay must
/// stay loaded while collision queries or ground-beam projections use it.
extern WorldCollisionGrid gShelterB6TrainingRoomCollisionGrid;

/// Authored cameras for mapped view indices 1..8, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gShelterB6TrainingRoomViewCameras[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT];

/// Sprite descriptors for mapped training-room views 1..8.
///
/// Views 3 and 7 contain the two enemy parts' destroyed-state overlays;
/// other views retain empty batch lists. Every view keeps decoded backdrop
/// strips and has no clipping list. Batches stay writable for part visibility
/// changes and all borrowed storage lives with this room overlay.
extern SpriteView gShelterB6TrainingRoomSpriteViews[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT];

/// The training room's twelve point lights and twelve cone lights.
///
/// No directional lights are supplied. Every source contributes in all views;
/// positions and falloff radii use room-coordinate units, RGB uses Q12 and
/// cone openings use 4096 angle units per turn. Gameplay updates the borrowed
/// transforms and attenuation while the room overlay remains loaded.
extern WorldCoordRoomLights gShelterB6TrainingRoomLights;

extern WorldCollisionTrigger D_shelter_b6_training_room_80185780[];

extern WorldCollisionTrigger D_shelter_b6_training_room_80185A44[];

/// Minimum ambient RGB levels for logical training-room views 1..8.
///
/// Entry zero holds the count; entries 1..8 hold colours in 16 units per
/// 8-bit colour level. View 3 has the brighter minimum; other views use
/// (16, 16, 16). Storage is borrowed while this room overlay is loaded.
extern WorldCoordRoomAmbientEntry gShelterB6TrainingRoomAmbientByView[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT + 1];

/// Collision and footstep policies indexed by training-room surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback.
/// Classes 1 and 2 have distinct footstep cue tables; classes 0 and 3..7
/// share the silent record. The directory and records live with this overlay.
extern WorldCollisionSurfaceProperties* gShelterB6TrainingRoomSurfaceProperties[8];

/// Runs the training-room controller's initialization, update and teardown states.
///
/// Exported to the Neo Ark map's area task table. `task->state` must be 0..2:
/// 0 registers the room message handlers and initializes the backdrop/scene
/// state, 1 waits for the defeat scene to finish before departure, and 2 kills
/// the task. Dispatch is unchecked; the room overlay must remain loaded.
void shelterB6TrainingRoomRoomTask(Task* task);

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
