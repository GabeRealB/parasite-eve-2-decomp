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

/// Occasionally emits a slow flame from the live scene actor's model part 3.
///
/// Advances the shared LCG once; an upper-half draw divisible by three emits
/// one flame after a second draw. Its sizing numerator is 256..767 and its
/// negative spawn flag selects generated drift of eight coordinates per frame.
/// Uses the fixed part-local offset (0, -576, 1600). Requires the scene-slot
/// task, its model child and gameplay's effect controller to remain live.
void dryfieldNightMotelBalconyTrySpawnFlame(void);

/// Emits eight flames and six smoke puffs around the scene actor's model part 3.
///
/// Requires the live scene-slot task, its model child and effect controller.
/// Each offset uses three successive LCG draws: X -127..128, Y -639..-384,
/// Z 1409..1664 in part-local coordinates. Flames use a fourth draw for size
/// 768..1279 and default generated drift. Offsets are copied during spawning;
/// the unused vector halfword is not initialized or consumed.
void dryfieldNightMotelBalconySpawnFlameBurst(void);

/// Updates the scenery sprite cues for the current Burner scene view.
///
/// Requires loaded writable sprite batches for the session's sprite variant.
/// Views 17/19 choose a batch with zero versus nonzero `sceneStep`; view 18
/// reveals the breakaway group. View 19 also applies view 22's cue: steps 1/2
/// select alternate batches and other values hide both. Other views do nothing.
void dryfieldNightMotelBalconyUpdateSceneSprites(u8 sceneStep);

/// Applies and saves one balcony section's damage-dependent sprite visibility.
///
/// `sectionIndex` is 0..8. Sections 0/1 accept `sectionState` 0..2; sections
/// 2..8 accept 0..1. Zero is intact; later states hide damaged scenery batches.
/// Requires the current stage/area's first sprite variant loaded and writable.
/// The selected byte-pair command stream supplies view indices and exact hidden
/// bytes; it is borrowed for this call. Indices and saved states are unchecked.
void dryfieldNightMotelBalconySetSectionState(s16 sectionIndex, s16 sectionState);

/// Reapplies all nine saved balcony section states to loaded sprite batches.
///
/// Requires the same stage/area tables and valid per-section state ranges as
/// `dryfieldNightMotelBalconySetSectionState`. Rewrites each saved nibble with
/// its current value while applying the first sprite variant's scenery.
void dryfieldNightMotelBalconyRestoreSectionStates(void);

/// Restores the Burner scene's sprite cues to their initial visibility.
///
/// Requires loaded writable sprite batches for the session's sprite variant.
/// Reveals view 17's two cues and hides the cue groups in views 18, 19 and 22.
/// The actor calls this before restoring the separately saved section states.
void dryfieldNightMotelBalconyResetSceneSprites(void);

/// Queues an axis-aligned textured billboard for one Burner breath flame.
///
/// `packedScreenPosition` holds signed pixel X/Y in its low/high halfwords.
/// `otIndex` counts writable tags in the current ordering table. The caller
/// projects and rejects invalid GTE results; this drawer does no clipping.
/// Nonnegative `animationStep` repeats atlas frames 2..11. With Q12 projected
/// scale S, horizontal half-width is (31*S)>>12 and the bottom extent is
/// (31*S)>>13 pixels; the top extends three times as far above the anchor.
/// Requires one packet's free arena space through GPU completion. Pixel stores
/// narrow to signed halfwords; no clamping or allocation failure check occurs.
void dryfieldNightMotelBalconyDrawBreathFlame(s32 packedScreenPosition, s16 otIndex, s16 projectedScaleQ12, s16 animationStep);

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

/// Draws visible lamp glows and emits the balcony's timed scene effects.
///
/// Bank-6 room effect 0x119 borrows live `EffectWork` in spawnArg2 and a coordinate
/// body. Requires the current view (0..63), composed view matrices, glow points,
/// packet/scratch space and room-effect state. The work's `angle` and `scale`
/// halfwords serve as scene tick counters, reset by the view-specific sequence.
/// Lamp-break state 1 emits a burst and becomes 2; either disables its glow.
/// This persistent handler draws and advances counters even during effect pause.
void dryfieldNightMotelBalconyAmbientEffectsTask(Task* task);

/// Runs balcony room setup, follow-up scene polling or task release.
///
/// The live bodyless task's state must be 0 setup, 1 poll or 2 release; dispatch
/// is unchecked. Requires the loaded room/session, event and sprite resources.
void dryfieldNightMotelBalconyRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
