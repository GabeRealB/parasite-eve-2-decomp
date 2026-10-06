#ifndef INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
#define INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_observatory_80181264[19];

// acropolis_observatory
extern WorldCollisionRoomResources D_acropolis_observatory_8017FEC8[];

extern u8* D_acropolis_observatory_8017FEF0[];

extern ViewCount D_acropolis_observatory_8017FEF8[];

extern WorldCoordRoomLighting D_acropolis_observatory_8017FEFC[];

extern DirectionWarpEntry D_acropolis_observatory_8017FF0C[];

extern SpriteView D_acropolis_observatory_80183300[];

extern ViewCamera D_acropolis_observatory_80183360[];

extern WorldCollisionSurfaceProperties* D_acropolis_observatory_801834DC[];

void func_acropolis_observatory_8017E6F8(Task* task);

/// Draws one frame of the observatory's additive ambient glow, then retires its effect.
///
/// Requires a live coordinate body and counted `EffectWork` in `spawnArg2.pointer`
/// from the effect spawner. The composed view position narrows to signed 16-bit
/// coordinates before projection through `GsWSMATRIX`. A negative
/// GTE FLAG suppresses drawing; otherwise the square uses a pixel half-extent
/// of 0x5D00 / max(SZ3 / 4 - 64, 16) and alternates two palettes each frame.
///
/// Requires an initialized scratch stack with room for one `EffectCentreScratch`,
/// a current depth ordering table, and room for one `POLY_FT4` in the frame arena.
/// Releases scratch and the counted work, and kills the task even when drawing
/// is suppressed. A queued packet remains live through GPU drawing.
void acropolisObservatoryAmbientGlowTask(Task* task);

void func_acropolis_observatory_8017D950(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
