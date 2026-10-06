#ifndef INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
#define INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_promenade_80183020[17];

// acropolis_promenade
extern WorldCollisionRoomResources D_acropolis_promenade_80181B90[];

extern u8* D_acropolis_promenade_80181BC0[];

extern ViewCount D_acropolis_promenade_80181BC8[];

extern WorldCoordRoomLighting D_acropolis_promenade_80181BCC[];

extern DirectionWarpEntry D_acropolis_promenade_80181BDC[];

extern SpriteView D_acropolis_promenade_80185FB4[];

extern ViewCamera D_acropolis_promenade_80186050[];

extern WorldCollisionSurfaceProperties* D_acropolis_promenade_801862B0[];

void func_acropolis_promenade_8017E03C(Task* task);

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
void acropolisPromenadeGlowStarTask(Task* task);

/// Smears a one-pixel framebuffer row down each frame to draw one screen drip.
///
/// Requires a live counted effect with owned, initially zeroed `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1.value` is the mapped camera view to retain.
/// Seeds X in 0..239, initial Y in 176..239, lifetime in 30..119 frames,
/// width in 16..79 pixels and fall speed in 1..4 frames per row. `move` holds
/// the initial pixel position, `scale` the lifetime, `angle` the width and
/// `period` the frames per row. Copies only source rows below 239 within the
/// current draw buffer, into a row-derived ordering-table slot (41..80).
/// Requires one `DR_MOVE` packet per visible frame; packets live until GPU
/// completion. Releases work and task on a view change, expiry or bottom edge.
void acropolisPromenadeScreenDripTask(Task* task);

/// Draws a flickering additive square in the coordinate body's local XZ plane.
///
/// Requires a live counted effect with a single-coordinate body and owned
/// `EffectWork` in `spawnArg2.pointer`. Composes the coordinate and transforms
/// the 768-unit half-side through its world matrix, narrowing corners to s16.
/// Copies the spawn phase to `age`, but it does not affect this one-frame draw.
/// Uses the 40-by-40 texture cell at (0, 16), with random grey in 0..15; draws
/// when the last corner's SZ3 / 4 plus 32 is at least 17. Requires initialized
/// GTE, 36 scratch-stack bytes and one `POLY_FT4` in the current frame arena,
/// consumed even when clipped. Releases scratch, work and task on return;
/// queued packets remain live until GPU completion.
void acropolisPromenadeGroundGlowTask(Task* task);

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
void acropolisPromenadeGlowLampTask(Task* task);

void func_acropolis_promenade_8017DA4C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
