#ifndef INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
#define INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_acropolis_sanctuary_8018402C[12];

extern TmdSource gAcropolisSanctuaryModel090F0;

extern TmdSource gAcropolisSanctuaryModel09584;

// acropolis_sanctuary
extern WorldCollisionRoomResources D_acropolis_sanctuary_801827EC[];

extern u8* D_acropolis_sanctuary_801827FC[];

extern ViewCount D_acropolis_sanctuary_80182800[];

extern WorldCoordRoomLighting D_acropolis_sanctuary_80182804[];

extern DirectionWarpEntry D_acropolis_sanctuary_8018280C[];

extern SpriteView D_acropolis_sanctuary_801860C8[];

extern ViewCamera D_acropolis_sanctuary_80186188[];

extern WorldCollisionSurfaceProperties* D_acropolis_sanctuary_801863F8[];

void func_acropolis_sanctuary_8017E00C(Task* task);

void func_acropolis_sanctuary_8017E134(Task* arg0);

void func_acropolis_sanctuary_8017E338(Task* arg0);

void func_acropolis_sanctuary_8017EC90(Task* arg0);

/// Draws the sanctuary's view-gated, flickering additive flame billboard.
///
/// Bank-6 slot 0x8B requires a live coordinate body and the spawner's owned
/// `EffectWork` in `spawnArg2.pointer`. In `spawnArg1.value`, bits 0..3 select
/// a placement (0..11), bits 8..9 a texture/palette variant (0..2), and bits
/// 16..27 a size factor (1..4095, or 0 for 640). Other bits are ignored. The
/// texture math addresses four 40-by-40 cells, but the grey tables cover only
/// variants 0..2; the room's spawns select 0 and 2, never 1 or 3. The current
/// view is 1..16 and each placement's mask determines whether it is drawn.
///
/// Initialization waits for the first visible update. It retains the placement
/// in the spawn word and caches size, variant, base grey and odd-frame flicker
/// in the work's `scale`, `angle`, `period` and `step`. Composed position is
/// narrowed to signed 16-bit coordinates and projected through `GsWSMATRIX`.
/// Half-extent is size factor * 39 / depth pixels, with depth SZ3 / 4; depths
/// below 17 reserve a packet but queue nothing. GTE projection flags are not tested.
///
/// Requires initialized projection settings, the flame texture and palettes,
/// one aligned `RoomGlowSpriteScratch` block and arena room for one `POLY_FT4`
/// on each visible update. Scaled depth wraps into the current ordering table;
/// queued packets live through GPU completion. Scratch is released every call;
/// the task and effect work persist until external teardown. Keep this room
/// overlay and the task's coordinate alive through every callback.
void acropolisSanctuaryFlameTask(Task* task);

void func_acropolis_sanctuary_8017D9E8(Task* task);

void func_acropolis_sanctuary_80180264(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
