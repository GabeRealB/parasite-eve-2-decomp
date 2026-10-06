#ifndef INCLUDE_ROOMS_SHELTER_R48_H
#define INCLUDE_ROOMS_SHELTER_R48_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern WorldCollisionGrid D_shelter_r48_80183EEC[1];

extern TaskDesc D_shelter_r48_80182FAC;

extern AreaVariant D_shelter_r48_8018BC10[13];

// shelter_r48
extern WorldCoordRoomLighting D_shelter_r48_80183014[];

extern WorldCollisionRoomResources D_shelter_r48_8018301C[];

extern u8* D_shelter_r48_8018302C[];

extern ViewCount D_shelter_r48_80183030[];

extern DirectionWarpEntry D_shelter_r48_80183034[];

extern ViewCamera D_shelter_r48_80183F10[];

extern SpriteView D_shelter_r48_80189FB4[];

extern WorldCollisionSurfaceProperties* D_shelter_r48_8018BE10[];

/// Dispatches Shelter R48's room setup, idle and teardown states.
///
/// Requires a live room task with state 0 (setup), 1 (idle) or 2 (kill).
/// Setup installs the room's message table, registers the room task slot and
/// starts the entry scene before advancing to idle. State 2 tears down the
/// task and its children. The room and its scene actor overlay must be loaded; no state bounds
/// check is performed.
void shelterR48RoomTask(Task* task);

/// Shows or hides the room's selected background-sprite batches across seven views.
///
/// `visible` is 0 to hide, 1 to show; other byte values leave them unchanged.
/// Requires Shelter R48's mutable sprite resources to be installed for the
/// current stage and area. The selected view indices are 1,2,3,5,6,7 and 17
/// in the zero-based view array; no resource or index validation is performed.
void shelterR48SetBackgroundSpritesVisible(u8 visible);

/// Initializes the ring texture phases and draws the room's progress-dependent glow.
///
/// Bank-6 effect 0x14C requires a live task, room-effect state and loaded room
/// resources. State 0 disables ground traces, consumes 96 LCG updates to seed
/// six bands of sixteen phase bytes, sets state 1 and also draws that tick.
/// Later ticks preserve the phases. Mapped views 3, 4, 6, 7, 8 and 18 draw an
/// orange glow at progress nibble 0x100 value 1, a blue glow at 2 and none at
/// other values. Radius scale is 256; both packed tints use flicker shift 5.
/// Requires composed view coordinates, scratch and primitive capacity. The
/// task does not inspect suspension/cancellation or retire itself; room
/// teardown owns its lifetime.
void shelterR48InitRingsAndDrawGlowTask(Task* task);

void func_shelter_r48_8017E4C4(Task* arg0);

void func_shelter_r48_8017EC18(Task* task);

/// Advances one counted eight-cell water-spray particle using this room's drawers.
///
/// Requires owned, initialized `EffectWork` in `spawnArg2.pointer`, a coordinate
/// body and initial state/cell zero. `spawnArg1` bits 0..11 give size, 12..15
/// updates per cell (0 selects 1), 16..23 launch speed (0 selects 64), 24..27
/// velocity kind (0 stationary, 1 upward burst, 2 all-axis spray, 3 narrow
/// upward jet, 5 copied position direction; others leave a zero direction),
/// and any bits 28..31 select the upright drawer. A supplied
/// nonzero move is already a velocity; generated directions are normalized
/// and scaled. Initialization draws nothing; later running updates draw,
/// move in parent-coordinate units and add 6 to Y velocity with halfword wrap.
/// Suspended updates redraw without aging; cancellation releases immediately.
/// Retirement frees work/body/task and decrements the effect count. Requires
/// composed drawing resources, scratch and primitive space; spin is in 4096
/// units per turn.
void shelterR48WaterDriftTaskU16(Task* task);

/// Advances Shelter R48's drifting sprite with sign-selected palettes and fixed Y acceleration.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. Bits 0..11 of `spawnArg1.value` give size, 12..14 give
/// period (1 only when bits 12..15 are all zero), 16..23 give speed (0 means 64),
/// and 24..27 select movement kind. A nonzero period nibble must encode a
/// nonzero period. Any bit in 28..31 selects the ten-cell alternate running
/// drawer; otherwise the twelve-cell banked drawer runs. Bit 31 selects palette
/// 1 (CLUT 0x428F), with palette 0 otherwise. Spin uses 4096 units per turn.
/// Initializes without drawing; moving updates advance position in coordinate
/// units then subtract 2/1 from banked/alternate Y velocity, even for kind 7.
/// Nonzero effect control freezes updates and selects the redraw's drawer from
/// bit 31 alone; control >= 4 frees work and task after the redraw. Drawing
/// requires composed coordinates, initialized scratch and primitive-packet space.
void shelterR48SpriteDriftTask(Task* task);

void func_shelter_r48_8017E704(Task* arg0);

void func_shelter_r48_8017E9B8(Task* arg0);

/// Expands and fades the three textured bands of the room's ring-wall effect.
///
/// Requires a counted coordinate-body effect with owned `EffectWork` and an
/// already composed coordinate. Its angle/step are radii in coordinate units,
/// period is the retained lift, and scale's low byte is brightness. A fresh
/// running tick sets brightness 128 and draws at 120; each running draw adds 512 to
/// base radius and 256 to spread and subtracts 8 from brightness. Suspension
/// redraws all three bands without aging. Cancellation redraws once, then
/// frees work/body/task and decrements the effect count; natural exhaustion
/// also retires the effect. Requires initialized texture-phase rows, scratch
/// and primitive space.
void shelterR48RingWallTask(Task* task);

void func_shelter_r48_801810B0(Task* task);

/// Expands three shockwave bands with an orange screen tint, then fades them.
///
/// Requires a counted coordinate-body effect with owned `EffectWork` and a
/// live borrowed parent. Initialization attaches an identity transform to the
/// parent, composes once and sets brightness 128 without drawing. Running
/// updates grow radius/lift/spread by 16 coordinate units; age 49 starts an
/// eight-update fade by 16. Its local transform stays fixed; the render pass
/// refreshes the composed transform with the parent. Suspended
/// updates redraw the bands without tint or aging; cancellation redraws once
/// before retiring. Retirement frees work/body/task and decrements the count.
/// Requires initialized texture-phase rows, scratch and primitive space.
void shelterR48ShockwaveRingsTask(Task* task);

/// Charges a pink ring flash, fades its disc and separates two shrinking glow beams.
///
/// Requires a counted coordinate-body effect, owned zero-initialized
/// `EffectWork` and a live parent. Initialization attaches and composes an
/// identity transform, overwrites spawnArg1 with a 30-running-update countdown
/// and immediately draws the first charge tick. The local transform stays
/// fixed; the render pass refreshes it with the parent. Angle is the disc
/// radius; period later holds beam radius and step
/// the mirrored beam yaws (4096 units per turn). Suspension freezes geometry
/// and brightness, but age still advances and a completed flash may change
/// state; cancellation frees work/body/task without drawing. Natural beam
/// exhaustion also retires the effect and decrements the count. Requires
/// scratch and primitive capacity, and nonzero projected beam-end depths.
void shelterR48PinkRingFlashTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R48_H
