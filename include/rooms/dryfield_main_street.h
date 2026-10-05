#ifndef INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_main_street_80184F20[13];

// dryfield_main_street
extern WorldCollisionRoomResources D_dryfield_main_street_80181BBC[];

extern u8* D_dryfield_main_street_80181BCC[];

extern ViewCount D_dryfield_main_street_80181BD0[];

extern WorldCoordRoomLighting D_dryfield_main_street_80181BD4[];

extern DirectionWarpEntry D_dryfield_main_street_80181BDC[];

extern ViewCamera D_dryfield_main_street_80182CC0[];

extern SpriteView D_dryfield_main_street_80184308[];

extern WorldCollisionSurfaceProperties* D_dryfield_main_street_801855EC[];

/// Animates and drifts one daytime main street puff (effect 0x601B1).
///
/// Requires the counted effect task and zeroed `EffectWork` created by
/// `Gp_SpawnEff`, with a live coordinate body and work in `spawnArg2.pointer`.
/// `spawnArg1.value` packs size factor in bits 0-11 (0..4095), cell period in bits
/// 12-14 (1..7 ticks), and speed in bits 16-23 (coordinate units per tick).
/// A zero period nibble (bits 12-15) selects one tick; a zero speed byte selects
/// 64. A nonzero period nibble must have nonzero bits 12-14; bit 15 alone
/// decodes to zero and is invalid. Bits 24-31 are ignored.
///
/// Initializes a fixed random screen angle (4096 units per turn) and a drift
/// with nonpositive local X, zero Y and signed Z. Draws before moving; cell zero
/// lasts one tick and cells 1-9 each last the selected period. Releases its work
/// and task after 1 + 9 * period ticks. Normal scheduling must compose the dirty
/// coordinate between ticks; callers must discard the work pointer on release.
void dryfieldMainStreetPuffTask(Task* task);

void func_dryfield_main_street_8017EEE8(Task* task);

void func_dryfield_main_street_8017F94C(Task* task);

void func_dryfield_main_street_80180234(Task* task);

void func_dryfield_main_street_8017E4B0(Task* task);

void func_dryfield_main_street_8017E168(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
