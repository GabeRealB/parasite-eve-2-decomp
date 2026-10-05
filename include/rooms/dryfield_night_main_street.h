#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H

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

extern SVECTOR gDryfieldNightMainStreetCollision06F80Normals[23];

extern SVECTOR gDryfieldNightMainStreetCollision06F80Verts[196];

extern WorldCollisionGridFace gDryfieldNightMainStreetCollision06F80Faces[89];

extern WorldCollisionTrigger D_dryfield_night_main_street_8018824C[12];

extern AreaVariant D_dryfield_night_main_street_80188A08[13];

// dryfield_night_main_street
extern WorldCollisionRoomResources D_dryfield_night_main_street_80182284[];

extern WorldCoordRoomLighting D_dryfield_night_main_street_801822B4[];

extern u8* D_dryfield_night_main_street_801822FC[];

extern ViewCount D_dryfield_night_main_street_80182308[];

extern DirectionWarpEntry D_dryfield_night_main_street_80182310[];

extern ViewCamera D_dryfield_night_main_street_80184564[];

extern SpriteView D_dryfield_night_main_street_801875E4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_main_street_80188B84[];

void func_dryfield_night_main_street_8017E484(Task* task);

/// Animates and drifts one nighttime main street puff (effect 0x601B2).
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
void dryfieldNightMainStreetPuffTask(Task* task);

void func_dryfield_night_main_street_8017FA68(Task* task);

void func_dryfield_night_main_street_801807B0(Task* arg0);

void func_dryfield_night_main_street_80180B48(Task* arg0);

void func_dryfield_night_main_street_80181F58(Task* arg0);

void func_dryfield_night_main_street_8017E0C0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
