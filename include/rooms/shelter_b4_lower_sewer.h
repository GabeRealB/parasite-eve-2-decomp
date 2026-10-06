#ifndef INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_lower_sewer_8018210C[4];

extern SVECTOR D_shelter_b4_lower_sewer_8018211C[10];

extern s16 D_shelter_b4_lower_sewer_80181E6C;

extern AreaVariant D_shelter_b4_lower_sewer_80183D48[12];

// shelter_b4_lower_sewer
extern u8* D_shelter_b4_lower_sewer_80181FA4[];

extern ViewCount D_shelter_b4_lower_sewer_80181FA8[];

extern DirectionWarpEntry D_shelter_b4_lower_sewer_80181FAC[];

extern WorldCollisionGrid D_shelter_b4_lower_sewer_801828E4;

extern ViewCamera D_shelter_b4_lower_sewer_80182908[];

extern SpriteView D_shelter_b4_lower_sewer_80182E80[];

extern WorldCoordRoomLights D_shelter_b4_lower_sewer_8018342C;

extern WorldCollisionTrigger D_shelter_b4_lower_sewer_80183444[];

extern WorldCollisionTrigger D_shelter_b4_lower_sewer_801837D4[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_lower_sewer_80183DF4[];

void func_shelter_b4_lower_sewer_8017D6D4(Task* task);

/// Runs the lower sewer's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, initial state 0 and owned
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is a positive charge
/// duration in running updates, consumed as a countdown. Nonzero room effect
/// controls pause without drawing; controls 4 and above, state 3 or fade
/// completion release the work, task and body and decrement the effect count.
/// Coordinate ancestors and this overlay must remain live, with initialized
/// scratch storage and frame packet space available for drawing.
void shelterB4LowerSewerRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws the lower sewer's fading blue twin trails.
///
/// Requires a coordinate body, zeroed counted `EffectWork` in `spawnArg2.pointer`,
/// initial state 0, `Task::work` initially NULL and a live `EffectWork::parent`.
/// Owns two eight-coordinate world-space histories in `Task::work`; allocation
/// failure retries initialization. The endpoints use this room's fixed local
/// offsets from the parent. `spawnArg1.value` 0 leaves the lifetime unlimited;
/// 2..32767 retire at that signed-halfword age in updates with control below 2.
/// Control values 2 and above skip updates and drawing, including retirement.
/// Teardown frees both allocations and the body and decrements the effect count.
/// The parent, view coordinate and overlay must remain live, with initialized
/// scratch storage and frame packet space available for drawing.
void shelterB4LowerSewerRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b4_lower_sewer_801811FC(Task* task);

/// Advances and draws an expanding, fading water-surface ripple for the lower sewer.
///
/// Requires a counted effect with a coordinate body, zeroed owned `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units (0..4095); higher bits are ignored.
/// Each running update grows it by 32, draws, then dims by 2, for 32 draws at
/// brightness 64..2. The first update sets a random yaw, composed on the next.
/// Non-running controls redraw retained values; cancellation draws once before
/// retirement. Retirement frees the work, task and body and decrements the effect
/// count. Coordinate ancestors, this overlay and its water textures must remain
/// live, with initialized scratch storage and frame packet space for drawing.
void shelterB4LowerSewerWaterRippleTask(Task* task);

/// Advances and draws an eight-cell water-spray particle for the lower sewer.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and animation index 0. `spawnArg1` bits
/// 0..11 give perspective size (0..4095), 12..15 updates per cell (0 selects 1),
/// and 16..23 launch speed in parent-coordinate units per update (0 selects 64).
/// Bits 24..27 choose direction (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 spawn-offset direction; others leave it zero).
/// A supplied nonzero `EffectWork::move` bypasses generation and scaling.
/// Nonzero bits 28..31 select an upright tile; otherwise a random angle is
/// retained at 4096 units per turn. The drawers receive unsigned cell indices.
///
/// The first running update initializes without drawing or moving. Later updates
/// draw, move and add 6 to signed-halfword Y velocity, retaining its low 16 bits;
/// stationary particles skip movement and gravity. Cells 0..7 each last the
/// decoded period. Suspended controls redraw; cancellation retires without drawing.
/// Retirement frees the work, task and body and decrements the effect count.
/// Coordinate ancestors, this overlay and its water textures must remain live,
/// with initialized scratch storage and frame packet space for drawing.
void shelterB4LowerSewerWaterDriftTaskU16(Task* task);

/// Installs the lower sewer's actor-effect IDs and draws its visible capsule glows.
///
/// Initial state 0 installs the flash, twin-trail and spark-burst IDs once, plus
/// the water IDs when the reservoir-completion flag is exactly 1. Every tick
/// draws additive grey glows for mapped views 2..9; view 1 draws none. Requires
/// this loaded room, a composed view matrix, initialized scratch storage and
/// current frame packet space and ordering table. Body and spawn arguments are
/// unused; the task stays live independently of room effect control.
void shelterB4LowerSewerDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
