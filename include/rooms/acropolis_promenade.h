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

void func_acropolis_promenade_8017E394(Task* task);

void func_acropolis_promenade_8017ED44(Task* task);

void func_acropolis_promenade_8017F0BC(Task* task);

void func_acropolis_promenade_8017DA4C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
