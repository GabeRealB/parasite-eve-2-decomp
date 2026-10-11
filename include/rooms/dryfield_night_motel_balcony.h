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

/// Saved-area change applied when the Burner begins its scripted balcony turn.
///
/// Selects night-balcony placement variant 4 in every save mode, removes its
/// saved enemy poses and clears its map mark. The second record is AREA_APPLY_END.
/// The actor borrows this room-owned list only during synchronous application.
extern const AreaApplyRec gDryfieldNightMotelBalconyBurnerTurnAreaUpdates[2];

/// Dryfield night motel balcony placement/resource directory indexed by `GameLocationKey.variant` 0..12.
///
/// Slots 1, 2, 3, 11 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightMotelBalconyAreaVariants[13];

/// Lighting descriptors for the balcony's three 1-based room variants.
///
/// Indexed by room minus one. Rooms 1 and 3 share their point lights and
/// ambient table; room 2 uses an alternate collection and ambient minima.
/// The nighttime Dryfield map borrows this directory and its room-owned data;
/// keep this overlay loaded while coordinate setup or lighting queries use it.
extern WorldCoordRoomLighting gDryfieldNightMotelBalconyRoomLighting[3];

/// Collision resources for the balcony's three 1-based room variants.
///
/// Room minus one selects a record. Rooms 1 and 2 share their grid; room 3
/// uses a separate mesh. All three share sight occluders and have distinct
/// view-boundary and action-trigger lists. Nighttime Dryfield area 29 borrows
/// this directory; keep the overlay and linked mutable resources loaded.
extern WorldCollisionRoomResources gDryfieldNightMotelBalconyRoomCollisionResources[3];

/// Per-room logical-view maps for the balcony's three room variants.
///
/// Room minus one selects a byte map; logical view minus one selects its
/// 1-based camera/image/sprite index. Each room searches 39 logical views.
/// Room 1 borrows gameplay's identity map; rooms 2 and 3 borrow distinct
/// mutable room-owned maps. Both room maps store a final zero beyond that
/// domain. The directory and room maps remain live only while this overlay is
/// loaded; the nighttime Dryfield map borrows them without taking ownership.
extern u8* gDryfieldNightMotelBalconyViewMaps[3];

/// Logical-view reverse-search counts for the balcony's three rooms.
///
/// Room minus one selects a signed 16-bit count of 39 map bytes, independently
/// of the maps' stored capacities. There is no terminator. Nighttime Dryfield
/// area 29 borrows this room-owned array while the overlay is loaded.
extern ViewCount gDryfieldNightMotelBalconyRoomViewCounts[3];

/// Five warp endpoints for the nighttime motel balcony.
///
/// A 1-based warp ID minus one selects player/companion placements, departure
/// sounds, arrival view and optional map marker. Endpoints 1..5 start in
/// logical views 2, 6, 5, 4 and 5; endpoint 5 repeats endpoint 3's placement
/// with no sounds. Nighttime Dryfield area 29 borrows these records while this
/// room overlay is loaded. Positions use world game units; yaw uses 4096 per turn.
extern DirectionWarpEntry gDryfieldNightMotelBalconyWarpEntries[5];

/// Camera records for the balcony's 39 mapped views.
///
/// Mapped view minus one selects a record shared by all three room variants.
/// Rotation uses 4096 per unit; translation is the negated world-space camera
/// origin in game units and projection distance uses pixels. Nighttime Dryfield
/// area 29 borrows this writable array; keep it loaded until queued camera
/// application finishes. There is no count record or terminator.
extern ViewCamera gDryfieldNightMotelBalconyViewCameras[39];

/// Scenery sprite descriptors for the balcony's 39 mapped views.
///
/// Mapped view minus one selects borrowed source, batch and optional clipping
/// lists shared by the three room view maps. Views 1, 3, 13, 15, 16, 24, 27,
/// 28 and 34 carry empty source slots. Batches and clipping lists have their own
/// sentinels; this directory has none. Nighttime Dryfield area 29 borrows the
/// directory and writable backing lists while this room overlay is loaded.
extern SpriteView gDryfieldNightMotelBalconySpriteViews[39];

/// Collision and footstep records indexed by balcony surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 0 and 4..7 share a record. Classes 1 and 2 select distinct step
/// requests; class 3 retains its own record with the same requests as the
/// default. The nighttime Dryfield map borrows the directory, records and
/// cue tables only while this room overlay is loaded. There is no terminator.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelBalconySurfacePropertiesByClass[8];

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

/// Emits one of five short debris/puff patterns for a balcony break impact.
///
/// Requires the counted `EffectWork` and coordinate body made by `effectSpawn`.
/// `spawnArg1.value` selects 0 large debris, 1 small debris, 2 scattered puffs
/// followed by downward debris, 3 row-1 puffs followed by large debris, or
/// 4 row-1 puffs only. Other selectors leave an undispatched state and are invalid.
/// Initialization takes one active tick, then emissions take one or two ticks;
/// the following tick releases the controller's work and task. Spawned particles
/// live independently. Any nonzero room effect control below four pauses;
/// four or above cancels immediately. Keep the room effect callbacks loaded.
void dryfieldNightMotelBalconyBreakBurstTask(Task* task);

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

/// Drops an animated billboard toward coordinate Y=0 and fades before release.
///
/// Requires a counted, zero-aged `EffectWork` and live coordinate body from
/// `effectSpawn`; bits 0..11 of `spawnArg1` set the sizing numerator. State 0
/// resets rotation and rolls a frame phase 0..7 and downward speed 0..255 units
/// per active tick. State 1 adds six units per tick to speed until local Y>0,
/// emits one slow row-1 puff and settles in state 2. Ages 20..29 fade by twelve
/// intensity units per tick; age 30 releases the work and task.
/// Initialization runs while paused, then pause freezes age and motion; hidden
/// control suspends all updates, and control four or above cancels. Drawing uses
/// the composed position before that tick's movement. Spawned puffs are independent.
void dryfieldNightMotelBalconyFallingParticleTask(Task* task);

/// Emits the broken balcony lamp's debris and puffs, then tapers away from its close view.
///
/// Requires counted, zero-aged `EffectWork` and a coordinate body from
/// `effectSpawn`; `spawnArg1` is ignored. Logical view 39 continuously rolls
/// independent debris, falling-particle and puff emissions, without advancing
/// age. Other views advance age toward 150 and emit progressively fewer puffs
/// through windows of 150 and 120 active ticks. Reentering view 39 preserves age.
/// Offsets are copied at spawn, in the controller's coordinate frame. Spawned
/// particles live independently. Nonzero room effect control below four pauses;
/// four or above cancels. Expiry or cancellation releases this work and task;
/// keep this room's callbacks and textures loaded while its particles remain.
void dryfieldNightMotelBalconyLampBurstTask(Task* task);

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
