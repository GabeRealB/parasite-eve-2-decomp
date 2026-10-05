#ifndef INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H
#define INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern u8 D_acropolis_bridge_80189A9C[24];

extern AreaVariant D_acropolis_bridge_8019004C[12];

// acropolis_bridge
extern WorldCollisionRoomResources D_acropolis_bridge_80189A54[];

extern u8* D_acropolis_bridge_80189A80[];

extern ViewCount D_acropolis_bridge_80189A88[];

extern WorldCoordRoomLighting D_acropolis_bridge_80189A8C[];

extern DirectionWarpEntry D_acropolis_bridge_80189AB4[];

extern SpriteView D_acropolis_bridge_8018FFA4[];

extern ViewCamera D_acropolis_bridge_80190A24[];

extern WorldCollisionSurfaceProperties* D_acropolis_bridge_80190C34[];

void func_acropolis_bridge_8017F868(Task* task);

void func_acropolis_bridge_801812F4(Task* task);

void func_acropolis_bridge_801819C8(Task* task);

void func_acropolis_bridge_80181D28(Task* task);

/// Draws a falling framebuffer strip starting on a random row 96..239.
///
/// `spawnArg2.pointer` owns `EffectWork`; `spawnArg1.value` selects the mapped view.
/// Age zero seeds `move.vx` and `move.vy` in screen pixels, `scale` as a 30..119-frame lifetime,
/// `angle` as a 16..79-pixel width and `period` as 1..4 frames per row. Each draw
/// copies one current-buffer row down by one pixel using the room's packet
/// cursor. A view change, expired lifetime or row >=239 releases work and task.
void acropolisBridgeFallingStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 72..239, with a near squared-depth ramp.
///
/// `spawnArg2.pointer` owns `EffectWork`; `spawnArg1.value` selects the mapped view.
/// Age zero seeds `move.vx` and `move.vy` in screen pixels, `scale` as a 30..119-frame lifetime,
/// `angle` as a 16..79-pixel width and `period` as 1..4 frames per row. Each draw
/// copies one current-buffer row down by one pixel using the room's packet
/// cursor. A view change, expired lifetime or row >=239 releases work and task.
void acropolisBridgeMidDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 104..239, with a low linear-depth ramp.
///
/// `spawnArg2.pointer` owns `EffectWork`; `spawnArg1.value` selects the mapped view.
/// Age zero seeds `move.vx` and `move.vy` in screen pixels, `scale` as a 30..119-frame lifetime,
/// `angle` as a 16..79-pixel width and `period` as 1..4 frames per row. Each draw
/// copies one current-buffer row down by one pixel using the room's packet
/// cursor. A view change, expired lifetime or row >=239 releases work and task.
void acropolisBridgeLowDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 72..239, with a farther squared-depth ramp.
///
/// `spawnArg2.pointer` owns `EffectWork`; `spawnArg1.value` selects the mapped view.
/// Age zero seeds `move.vx` and `move.vy` in screen pixels, `scale` as a 30..119-frame lifetime,
/// `angle` as a 16..79-pixel width and `period` as 1..4 frames per row. Each draw
/// copies one current-buffer row down by one pixel using the room's packet
/// cursor. A view change, expired lifetime or row >=239 releases work and task.
void acropolisBridgeTallDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on a random row 104..231.
///
/// `spawnArg2.pointer` owns `EffectWork`; `spawnArg1.value` selects the mapped view.
/// Age zero seeds `move.vx` and `move.vy` in screen pixels, `scale` as a 30..119-frame lifetime,
/// `angle` as a 16..79-pixel width and `period` as 1..4 frames per row. Each draw
/// copies one current-buffer row down by one pixel using the room's packet
/// cursor. A view change, expired lifetime or row >=239 releases work and task.
void acropolisBridgeParticleStreakTask(Task* task);

/// Expands and fades a water-surface ripple using the task's composed coordinate.
///
/// `spawnArg2.pointer` owns `EffectWork`; spawnArg1 bits 0..11 give the initial
/// half-side in coordinate units. State zero seeds brightness 64 and random
/// local Y rotation. Running updates grow half-side by 32, draw, then fade by
/// two: `angle` stores size and `scale` stores brightness (128 is neutral).
/// Frozen updates redraw; cancellation redraws once before releasing work and
/// task. `workm` must already be composed; this task does not rebuild its cache.
void acropolisBridgeWaterRippleTask(Task* task);

void acropolisBridgeEffectSpriteDebrisTask(Task* task);

/// Moves and draws one blue-green 1x1 dust mote at the task coordinate.
///
/// `spawnArg2.pointer` owns `EffectWork`; the coordinate belongs to `coordBody`.
/// First update seeds local velocity and colour. Composes before moving, so the
/// tile uses the cached pre-movement position. Only accepted depths add six
/// coordinate units per update to Y velocity. Age 31 or local Y >=-29 releases
/// work and task; this task does not consult the pause/cancel control.
void acropolisBridgeDustMoteTask(Task* task);

void func_acropolis_bridge_8017F788(Task* task);

void func_acropolis_bridge_8017DA0C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H
