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
/// `effectSpawn`, with a live coordinate body and work in `spawnArg2.pointer`.
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

/// Runs this room's animated mote with rising or steady vertical motion and a fade.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// `spawnArg1` packs half-extent in bits 0..11 (0..4095 world units), palette
/// in bits 12..15 (0 default), unsigned speed in bits 16..23 (coordinate units
/// per active tick), and signed lifetime in bits 24..31 (active ticks).
/// Bits 0..1 overlap the extent: either bit selects steady motion, with bit 1
/// selecting upward motion. Otherwise it rises at speed plus a random 0..63.
/// The first active tick only initializes; later ticks move along local Y and
/// draw on odd ages. Brightness fades by 16 after age exceeds lifetime minus 8;
/// the following tick releases the work and task once brightness is zero.
/// Nonzero room effect control pauses it; control 4 or above cancels it.
/// The controller and coordinate ancestors must stay live until release;
/// callers must discard the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsMoteTask(Task* task);

/// Runs this room's expanding tinted halo, shrinking ring and fading star.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// Initialization attaches to the work's borrowed parent at its saved local
/// offset in game coordinate units. `spawnArg1.halves.low` supplies 1..65535
/// expansion ticks; `halves.high` selects tint row 0..2. Initialization replaces
/// the argument word with the remaining ticks. Brightness and world-unit disc
/// radius grow by integer 256 / duration each active tick, then the star fades.
/// State 3 requests release. Nonzero room effect control pauses all phases,
/// including that request; control 4 or above cancels immediately.
/// Completion or cancellation releases the work and task. The controller and
/// borrowed coordinate parent must stay live until release; callers must
/// discard the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsHaloTask(Task* task);

/// Runs this room's growing orange disc and layered glow inside a fading ring.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// `spawnArg1` is unused. The first active tick initializes and draws; later
/// ticks expand the burst and fade its ring before fading the centre.
/// Nonzero room effect control pauses it; control 4 or above cancels it.
/// Completion or cancellation releases the work and task. The controller and
/// coordinate ancestors must stay live until release; callers must discard
/// the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_dryfield_night_main_street_80181F58(Task* arg0);

void func_dryfield_night_main_street_8017E0C0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
