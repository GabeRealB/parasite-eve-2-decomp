#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_motel_balcony_80182834[2];

extern AreaApplyRec D_dryfield_night_motel_balcony_8018F2CC[2];

extern AreaVariant D_dryfield_night_motel_balcony_8018EA94[13];

// dryfield_night_motel_balcony
extern WorldCoordRoomLighting D_dryfield_night_motel_balcony_80182E00[];

extern WorldCollisionRoomResources D_dryfield_night_motel_balcony_80182E18[];

extern u8* D_dryfield_night_motel_balcony_80182E98[];

extern ViewCount D_dryfield_night_motel_balcony_80182EA4[];

extern DirectionWarpEntry D_dryfield_night_motel_balcony_80182EAC[];

extern ViewCamera D_dryfield_night_motel_balcony_80184004[];

extern SpriteView D_dryfield_night_motel_balcony_8018D078[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_balcony_8018F2AC[];

void func_dryfield_night_motel_balcony_80182730(void);

void func_dryfield_night_motel_balcony_8018257C(void);

void func_dryfield_night_motel_balcony_8017E128(u8 arg0);

void func_dryfield_night_motel_balcony_8017E250(s16 arg0, s16 arg1);

void func_dryfield_night_motel_balcony_8017E3C8(void);

void func_dryfield_night_motel_balcony_8017E4B8(void);

void func_dryfield_night_motel_balcony_8017F6C8(s32 arg0, s16 arg1, s16 arg2, s16 arg3);

/// Updates a spinning debris particle that loses speed on collisions and fades.
///
/// Bank-6 handler for `EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS`. The task owns
/// the live `EffectWork` in spawnArg2 and a coordinate body parented to the view.
/// At spawn, bits 0..11 of spawnArg1 give the sizing numerator; bits 16..17
/// select texture/palette row 0..2 (3 is invalid). A negative word selects a
/// narrower, downward random launch. A preset nonzero move supplies a direction;
/// otherwise one is rolled and rotated by the spawn parent's local matrix.
/// The direction uses 4096 per unit; scale is displacement per frame.
///
/// States are 0 new, 1 moving, 2 settled. Fading starts at age 60 and the
/// particle is released at 90. Initialization runs while paused; pause then
/// freezes motion/age. Hidden room effect control suspends the handler, and
/// cancellation releases its work and task.
void dryfieldNightMotelBalconyDebrisTask(Task* task);

void func_dryfield_night_motel_balcony_80180580(Task* task);

/// Updates a drifting ten-frame flame particle and fades it before release.
///
/// Bank-6 handler for `EFFECT_NIGHT_MOTEL_BALCONY_FLAME`, with a live owned
/// `EffectWork` in spawnArg2 and a coordinate body parented to the view. Bits
/// 0..11 of spawnArg1 give the sizing numerator. Random drift has length 8 for
/// a negative word, 128 for bit 30, or 32 otherwise, in coordinates per frame.
/// A preset nonzero move instead uses the caller's scale. Generated directions
/// are rotated by the spawn parent's local matrix, normalized and scaled once.
///
/// State 0 chooses a start frame and lifetime (5..14 frames); state 1 moves
/// and draws. The last ten ages fade, including all drawn ages of short lives.
/// Initialization runs while paused; pause then freezes motion/age. Hidden
/// room effect control suspends the handler; cancellation or expiry releases
/// its work and task.
void dryfieldNightMotelBalconyFlameTask(Task* task);

void func_dryfield_night_motel_balcony_801809CC(Task* task);

void func_dryfield_night_motel_balcony_80181024(Task* task);

/// Updates a drifting puff through twelve animation frames, then releases it.
///
/// Bank-6 handler for `EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF`, with a live
/// owned `EffectWork` in spawnArg2 and a coordinate body parented to the view.
/// Bits 0..11 of spawnArg1 give the sizing numerator; bits 16..17 select
/// palette-origin row 0..2 (3 is invalid). Bit 30 selects scattered drift;
/// otherwise a negative word selects rising drift, and a positive word selects
/// horizontal drift with a small downward component. Generated drift has
/// length 64 with bit 30 or 29, or 128 otherwise, in coordinates per frame.
/// A preset nonzero move instead uses length 128. Generated directions are
/// rotated by the spawn parent's local matrix, normalized and scaled once.
///
/// State 0 chooses a frame period (1..4 ages) and replaces spawnArg1 with the
/// palette row; state 1 moves and draws until frame 12. Initialization runs
/// while paused; pause then freezes motion/age. Hidden room effect control
/// suspends the handler; cancellation or animation end releases its work and task.
void dryfieldNightMotelBalconyDriftPuffTask(Task* task);

void func_dryfield_night_motel_balcony_8017E554(Task* task);

void func_dryfield_night_motel_balcony_8017DD78(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
