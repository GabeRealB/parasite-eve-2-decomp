#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"

extern AreaVariant D_shelter_b1_pod_service_gantry_8017FBF8[11];

// shelter_b1_pod_service_gantry
extern u8* D_shelter_b1_pod_service_gantry_8017FB1C[];

extern ViewCount D_shelter_b1_pod_service_gantry_8017FB20[];

extern DirectionWarpEntry D_shelter_b1_pod_service_gantry_8017FB24[];

extern WorldCollisionGrid D_shelter_b1_pod_service_gantry_801801C4;

extern ViewCamera D_shelter_b1_pod_service_gantry_801801E8[];

extern SpriteView D_shelter_b1_pod_service_gantry_80181BA0[];

extern WorldCoordRoomLights D_shelter_b1_pod_service_gantry_801824F4;

extern WorldCollisionSurfaceProperties* D_shelter_b1_pod_service_gantry_80182520[];

/// Runs the pod service gantry's room receiver and two-scene sequence.
///
/// Requires a live task with state 0 (register and allocate scene work), 1
/// (advance the scene sequence), or 2 (kill). Start at 0; the task owns its
/// allocated work until teardown, and allocation failure kills the receiver.
/// Keep the room/map overlays and scene resources live while used. Running
/// state 1 requires the work initialized by state 0; the final sequence step
/// waits for the area transition to tear down the room.
void shelterB1PodServiceGantryRoomTask(Task* task);

/// Draws the additive disc at a chain end in its composed coordinate frame.
///
/// Borrows `coord` with a current world-space `workm`; transforms local point
/// (0, -196, 0), retaining signed halfword coordinates, then projects it through
/// the current view. `chainNumber` modulo eight selects the persistent flicker
/// phase. The signed low halfword of `radiusScale` gives screen radius
/// `radiusScale * 64 / (SZ3 / 4)` in pixels; accepted depth must be nonzero.
///
/// `packedColor` contains red, green and blue nibbles in bits 8..11, 4..7 and
/// 0..3, each expanded by 16. Bits 12..15 give the shift of a parity-controlled
/// increment added to all channels; packet bytes retain the low eight bits.
/// Requires actor-560800's tick, initialized flicker phases, scratch space for
/// one `EffectCentreScratch` and frame-arena capacity for four Gouraud quads
/// plus their additive draw-mode packets. Scratch is released before return;
/// queued packets remain live until the GPU consumes this frame.
void shelterB1PodServiceGantryDrawChainGlow(const GfxCoord* coord, s32 chainNumber, s32 radiusScale, s16 packedColor);

/// Seeds the chain-glow flicker offsets and selects the room's water-spray effect.
///
/// A live task in state 0 advances the shared random sequence eight times,
/// storing bits 16..23 as one byte per chain, then enters idle state 1.
/// Any nonzero state does nothing. Spawn arguments and the coordinate body are
/// unused; this callback allocates no work and leaves teardown to its owner.
void shelterB1PodServiceGantryInitEffectsTask(Task* task);

/// Advances and draws one counted eight-cell water-spray particle for this room.
///
/// Requires a live coordinate-body task in state 0 with owned, initialized
/// `EffectWork` in `spawnArg2.pointer`, initially at cell 0, and a live
/// `gRoomEffectState`. Translation and velocity use the coordinate's parent
/// space, normally view space. Drawing requires initialized scratch storage
/// and capacity for one textured-quad packet.
///
/// `spawnArg1.value` packs perspective size in bits 0..11, running updates per
/// cell in 12..15 (0 selects 1), launch speed in 16..23 (0 selects 64), and
/// velocity kind in 24..27 (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 the copied `pos` direction; others launch at zero).
/// Any bits 28..31 select an upright sprite; otherwise the random spin angle
/// stays fixed, in 4096 units per turn. A supplied nonzero `move` bypasses
/// direction generation and scaling. Velocity is in parent-coordinate units
/// per running update and its components retain signed halfword precision.
///
/// The first running update initializes without drawing or moving. Subsequent
/// updates draw before moving, add 6 to moving particles' Y velocity, and
/// retire after eight cells of the selected period. Suspended control values
/// below 4 redraw without aging; control >= 4 retires without drawing.
/// Retirement frees the work, decrements the effect count and tears down the
/// task and coordinate body; callers must not retain pointers after retirement.
void shelterB1PodServiceGantryWaterDriftTaskU16(Task* task);

/// Moves and draws the gantry's eight-cell rising sprite with a random palette.
///
/// Requires a counted coordinate-body task in state 0 with owned, zero-initialized
/// `EffectWork` in `spawnArg2.pointer` and a live `gRoomEffectState`. Bits 0..11 of
/// `spawnArg1.value` select the perspective size numerator; bit 16 reverses Y
/// motion, and other bits are ignored. The first running update chooses speed
/// 16..79 parent-coordinate units per update and a fixed screen angle in
/// 4096 units per turn. `move.vy` holds signed velocity, `scale` the angle,
/// and `angle` the size; these parameters retain signed halfword precision.
///
/// Running updates move along parent-space Y before drawing, dirty the transform
/// cache, and advance the cell every fourth update. Update 32 moves then releases
/// without drawing. Drawing uses the existing composed cache, requires scratch
/// and quad-packet capacity, and selects one of six palettes on every draw.
/// Controls 1..3 freeze initialization, motion and animation but retain drawing
/// and random consumption; controls >=4 release before drawing. Retirement frees
/// the work, decrements the effect count and tears down the task and body.
void shelterB1PodServiceGantryEffectSpriteRiseTask(Task* task);

/// Advances the pod service gantry's animated drifting sprite and releases it at completion.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` packs size in bits 0..11, period in
/// 12..14 (1 only when bits 12..15 are all zero), speed in 16..23 (0 means 64),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// A nonzero period nibble must encode a nonzero period. Spin uses 4096 units
/// per turn; movement uses coordinate units per running update.
/// Initializes without drawing; later updates draw before moving, accelerating
/// and advancing through 12 banked or 10 alternate cells. Nonzero effect control
/// freezes updates but retains the drawer and palette; control >= 4 frees work
/// and task after that redraw. Drawing needs composed coordinates and scratch
/// and primitive-packet capacity.
void shelterB1PodServiceGantrySpriteDriftTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
