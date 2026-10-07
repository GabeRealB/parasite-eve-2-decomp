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

/// Emits the bridge's view-dependent glows, player-joint water effects and framebuffer streaks.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, mapped view 1..10 and a live player model through part 17.
/// State zero installs the room-effect message table and snapshots parts 14/17.
/// View 9 raises star glows by 576 units and omits ground glows; lamp variants
/// follow their own emitter masks. In views 2/6/7 while running, joint travel
/// controls ripple/spray draws. The height gate retains its current part alias:
/// part 17 on initialization, the player root on later updates.
/// `age` wraps as the frame counter, `angle` holds a movement threshold and
/// `scale` stores the previous view. New streak views seed 30 copies; ongoing
/// emission depends on the view and battle state. Resets the frame's DR_MOVE
/// cursor in the borrowed 0x18000-byte file buffer; its two 0x7000-byte packet
/// regions begin at byte 0xA000. Cancellation requests leave the task alive
/// and emit nothing. Work, player/model, view tables and buffer must stay live.
void acropolisBridgeAmbientEffectsTask(Task* task);

/// Draws one frame of an animated lamp core and a rotating, flickering flare.
///
/// Requires a live counted effect with a single-coordinate body and owned
/// `EffectWork` in `spawnArg2.pointer`. The signed low halfword of `spawnArg1`
/// is the phase: its signed remainder modulo six selects the 16-texel core cell;
/// added to `gDisplayState.animFrame`, it gives the flare angle in 4096 units
/// per turn, narrowed to a signed halfword. Projects the composed centre through
/// the current view; camera Z / 4 below 17 queues neither quad. Requires initialized
/// GTE, 24 scratch-stack bytes and space for two `POLY_FT4` packets in the current
/// frame arena. Even a clipped draw consumes one packet. Always releases work
/// and task before returning; queued packets remain live until GPU completion.
void acropolisBridgeGlowStarTask(Task* task);

/// Draws the bridge's one-frame ground glow and releases its effect work.
///
/// Requires a coordinate body, owned and counted `EffectWork` in
/// `spawnArg2.pointer`, a composed current view and room for one `POLY_FT4`.
/// The low spawn halfword is copied to age but does not affect the draw.
/// The horizontal quad's rotated corners are all replaced with the composed
/// origin narrowed to signed halfwords, preserving the collapsed quad.
/// Queues it with a random grey in 0..15 when camera Z / 4 plus 32 is at least
/// 17. Consumes its packet even when clipped; queued packets live until GPU
/// completion. Borrows and releases 44 scratch-stack bytes before killing the
/// effect and freeing its work.
void acropolisBridgeGroundGlowTask(Task* task);

/// Draws one frame of a flickering additive lamp billboard and releases the effect.
///
/// Requires a live counted effect with a single-coordinate body and owned work
/// in `spawnArg2.pointer`. The full signed `spawnArg1.value` must be 0..2:
/// it selects a 40-by-40 texture cell and palette, with grey levels 32/40,
/// 96/112 or 32/44 on even/odd animation frames. Composes the coordinate and
/// narrows its composed origin to s16 before projection through `GsWSMATRIX`.
/// The composed transform includes the view parent. At camera Z / 4
/// >=17, queues a square of pixel half-side 24960 / depth; GTE flags do not
/// gate drawing. Requires initialized GTE, 20 scratch-stack bytes and space
/// for one `POLY_FT4` packet, consumed even when clipped. Always releases
/// scratch, work and task; queued packets remain live until GPU completion.
void acropolisBridgeGlowLampTask(Task* task);

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

/// Animates one eight-cell debris sprite used for the bridge's water spray.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`
/// and a single-coordinate body. The coordinate's `workm` must be composed for
/// drawing. Start at state 0 and cell `index` 0..7; states 1/2 draw chip/billboard.
/// Initialization takes one running update without drawing, moving or advancing.
///
/// `spawnArg1` bits 0..11 set perspective size (0..4095); bits 12..15 give
/// running updates per cell (0 means 1); bits 16..23 give launch speed in parent
/// coordinate units per update (0 means 64). Bits 24..27 choose direction:
/// 0 stationary, 1 random upward, 2 random on all axes, 3 narrow upward, 5 from
/// `work->pos`. Other kinds leave the zero direction for SDK normalization.
/// Any set bit in 28..31 selects the billboard. Spin is randomized once in
/// 4096 units per turn. A nonzero initial `move` is retained as velocity and
/// enables movement regardless of the packed speed or direction.
///
/// Running updates draw, move by signed halfword velocity, then add 6 to Y
/// velocity and advance the cell every period. Stationary sprites skip both
/// movement and gravity. Suspension redraws a chip without advancing; cancellation
/// redraws that chip once and releases the counted work and task. Finishing cell
/// 7 also releases them. Drawers require initialized GTE, scratch and packet state.
void acropolisBridgeEffectSpriteDebrisTask(Task* task);

/// Moves and draws one blue-green 1x1 dust mote at the task coordinate.
///
/// `spawnArg2.pointer` owns `EffectWork`; the coordinate belongs to `coordBody`.
/// First update seeds local velocity and colour. Composes before moving, so the
/// tile uses the cached pre-movement position. Only accepted depths add six
/// coordinate units per update to Y velocity. Age 31 or local Y >=-29 releases
/// work and task; this task does not consult the pause/cancel control.
void acropolisBridgeDustMoteTask(Task* task);

/// Runs the bridge keypad's entry, Examine prompt, code check and result display.
///
/// Start at state zero; the nine states allocate work and a cursor task, arm
/// the cursor, collect keys, open and poll Examine, check a complete code,
/// blink acceptance or rejection, and close. State must stay in 0..8. Closing
/// requests exit with result 1 for acceptance or 0 for cancellation/failure.
/// The keypad sprite view and action-prompt controller must remain loaded.
void acropolisBridgeKeypadTask(Task* task);

/// Runs the bridge room's setup, message-waiting idle state or teardown.
///
/// `task->state` must be 0..2. Setup installs room message handlers, publishes
/// the room task and spawns its bridge model; idle keeps the receiver live.
/// Called by the Acropolis map's bridge-area descriptor while this room is loaded.
void acropolisBridgeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H
