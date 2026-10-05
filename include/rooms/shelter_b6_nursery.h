#ifndef INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
#define INCLUDE_ROOMS_SHELTER_B6_NURSERY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB6NurseryModel07BAC;

extern AreaVariant D_shelter_b6_nursery_801874A4[13];

// shelter_b6_nursery
extern u8* D_shelter_b6_nursery_80185304[];

extern ViewCount D_shelter_b6_nursery_80185308[];

extern DirectionWarpEntry D_shelter_b6_nursery_8018530C[];

extern WorldCollisionGrid D_shelter_b6_nursery_801858A0;

extern ViewCamera D_shelter_b6_nursery_801858C4[];

extern SpriteView D_shelter_b6_nursery_80186FD0[];

extern WorldCoordRoomLights D_shelter_b6_nursery_80187294;

extern WorldCollisionTrigger D_shelter_b6_nursery_801872AC[];

extern WorldCollisionTrigger D_shelter_b6_nursery_8018750C[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_nursery_8018789C[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_nursery_80187958[];

void func_shelter_b6_nursery_8017FF9C(Task* task);

void func_shelter_b6_nursery_8017FFF4(void);

/// Shows or hides the second sprite batch in the nursery's view 13.
///
/// `hidden` is 0 to show or 1 to hide; other byte values leave it unchanged.
/// Requires the active session's stage and area to select the nursery sprite
/// tables. Changes view 13 regardless of the current view.
void shelterB6NurserySetView13SpriteHidden(u8 hidden);

/// Spins and draws one grey triangular shard of the nursery's spark shower.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// The low 12 bits of `spawnArg1.value` give its radius in local coordinate
/// units. Initializes a Q12 direction, speed 64..127, shade 64..191 and Euler
/// increments in 4096 units per turn. Releases the work when parent-space Y
/// exceeds zero. Room effect control 2..3 pauses it; 4 or above cancels it.
void shelterB6NurserySparkShowerShardTask(Task* task);

void func_shelter_b6_nursery_80182D14(s32 arg0, s32 arg1);

void func_shelter_b6_nursery_801800A0(Task* task);

void func_shelter_b6_nursery_80181314(Task* task);

/// Animates one nursery particle through a ten-frame or eight-frame texture strip.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// `spawnArg1.value` packs half-diagonal (bits 0..11), frame period (12..14,
/// default 1 when bits 12..15 are zero), speed (16..23, zero selects 64),
/// motion mode (24..27), and the eight-frame strip selector (bit 31).
/// A nonzero period is required: bit 15 alone must not select the period.
/// Motion modes are 0 stationary, 1 negative-Y fan, 2 scatter, 3 negative-Y
/// jet, 5 spawn-offset direction, 6 XZ scatter, and 7 parent-rotated jet.
/// Existing nonzero `move` bypasses direction generation and speed scaling.
/// Each frame draws before moving, then advances the strip at its period;
/// completion releases the work. This task runs regardless of room effect control.
void shelterB6NurseryAnimatedParticleTask(Task* task);

/// Runs the nursery's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// `spawnArg1.value` is a positive charge duration in active ticks, consumed
/// as a countdown. Nonzero room effect control pauses it; control 4 or above,
/// state 3, or completion releases the effect.
void shelterB6NurseryRoomVisualEffectsFlashTask(Task* task);

/// Records two anchor-relative endpoints and draws their fading trail as a beam.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`;
/// its parent coordinate stays live through teardown. `work` starts null and
/// owns sixteen allocated coordinate snapshots, eight for each endpoint.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish;
/// zero leaves lifetime to external teardown. Age advances with room effect
/// control below 2, including initialization; allocation failure resets it
/// for retry. Teardown releases both allocations.
void shelterB6NurseryRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b6_nursery_80184074(Task* task);

void func_shelter_b6_nursery_8017EAC4(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
