#ifndef INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_storeroom_80186C9C[22];

// shelter_b1_storeroom
extern WorldCollisionRoomResources D_shelter_b1_storeroom_80184B50[];

/// Authored lighting for the storeroom's single room, indexed by room minus one.
///
/// Borrows one directional light, four point lights and ambient minima for
/// logical views 1..8. Light transforms and attenuation are mutable; the room
/// overlay must stay loaded while gameplay holds these resources.
extern WorldCoordRoomLighting gShelterB1StoreroomRoomLighting[1];

/// Logical-view map directory for the storeroom's single room.
///
/// Indexed by room minus one; the borrowed gameplay identity map preserves
/// logical views 1..8 as camera, image and sprite indices. The directory stays
/// valid only while this room overlay is loaded.
extern u8* gShelterB1StoreroomRoomViewMaps[1];

extern ViewCount D_shelter_b1_storeroom_80184B6C[];

extern DirectionWarpEntry D_shelter_b1_storeroom_80184B70[];

/// Authored cameras for mapped view indices 1..8, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gShelterB1StoreroomViewCameras[8];

/// Foreground sprite resources for mapped views 1..8, indexed by view minus one.
///
/// Views 2..7 borrow mutable sources and sentinel-terminated batches; views 1
/// and 8 use image strips alone. No view supplies clipping commands. The room
/// overlay must stay loaded while allocating or drawing these resources.
extern SpriteView gShelterB1StoreroomSpriteViews[8];

/// Collision policies and footstep cues indexed by room-local surface class 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Class 1 supplies floor footstep cues; classes 0 and 2..7 share the silent
/// record. Gameplay borrows these records while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB1StoreroomSurfaceClassProperties[8];

/// Runs the storeroom's room-message receiver until room teardown.
///
/// Starts at state 0 to install the handlers, register the room task and enable
/// CAP completion sound messages. State 1 waits for messages; state 2 kills
/// the task. The 0..2 dispatch index is unchecked. Spawn payloads are unused;
/// keep this overlay loaded while the registered receiver is live.
void shelterB1StoreroomTask(Task* task);

/// Charges a pink flash, tints the screen at its peak, then fades as a star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.value` is a positive charge duration in active
/// ticks, consumed as a countdown after initialization. State 3 requests
/// release. Nonzero room effect control pauses every phase; four or above
/// cancels it. Completion or cancellation releases the work and task.
/// Coordinate ancestors, the effect controller and this overlay must remain
/// live; drawing needs the current frame's packet arena and scratch stack.
void shelterB1StoreroomRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue twin trail.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate and `Task::work` initially NULL.
/// Allocates two eight-coordinate histories in `Task::work`; allocation failure
/// retries initialization. Endpoints use fixed parent-local offsets (0, 190,
/// -15) and (0, 1085, 180) in game units. Snapshots preserve world positions as
/// the parent moves. `spawnArg1.value` zero leaves lifetime to parent teardown;
/// values 2..32767 release at that age in active ticks. Control 2 or above
/// pauses recording and expiry; control 1 still records and draws. Teardown
/// frees both work allocations. The parent, controller and overlay must remain live.
void shelterB1StoreroomRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Start in state 0 with the coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` supplied by `effectSpawn`. Nonzero `spawnArg1.value`
/// selects smoke; zero selects two sparks and fading rings. Active age seven
/// enters release; the next active tick frees work and kills the task.
/// Nonzero room effect control pauses it; four or above cancels it. Child effects
/// live independently. Keep the coordinate ancestors, effect controller, room
/// overlay and current graphics workspace live through teardown.
void shelterB1StoreroomRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs an attached charge disc with player-joint sparks and a fading release ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1.value` selects tint 0 or 1.
/// The initial state attaches at the work's copied offset in its borrowed
/// parent's axes. The owner may request flicker, release or cancellation with
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Growth emits sparks from player parts 3..18
/// every fourth active age and adopts them for teardown with the disc. Requires
/// the installed flying-spark effect, live player model and coordinate ancestors.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// frees work, children and task; keep the controller and room overlay loaded.
void shelterB1StoreroomRoomVisualEffectsGlowDiscTask(Task* task);

/// Flies an animated spark toward its target's initial position with a fixed step.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.pointer` borrows a target `GfxCoord`; both cached
/// transforms must be composed into the same view space at initialization.
/// The first active tick fixes the step at 204/4096 of the initial separation
/// in parent axes, narrowing components to s16. The target is not sampled
/// again. Later ticks move and draw on odd ages; age 20 releases work and task.
/// Nonzero room effect control pauses it; four or above cancels it. Coordinate
/// ancestors, the effect controller and this overlay must remain live; the
/// target is needed only for initialization.
void shelterB1StoreroomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Uses the flying-effect drawers and refreshes transient light slot 2.
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// or cancellation releases the work and task. Coordinate ancestors, the
/// effect controller and this overlay must remain live.
void shelterB1StoreroomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs a vertically drifting animated mote until it fades.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1` packs half-extent in bits 0..11, palette in bits
/// 12..15, unsigned speed in bits 16..23 and signed lifetime in bits 24..31.
/// Extent and speed use parent-coordinate units; lifetime uses active ticks.
/// Motion bits 0..1 overlap the extent: either selects steady motion, with
/// bit 1 selecting upward motion. Neither selects a brightening rise with
/// added random speed. Initialization draws nothing; later ticks draw on odd
/// ages. Nonzero room effect control pauses it; four or above cancels it.
/// Completion or cancellation releases the work and task. Coordinate ancestors,
/// the effect controller and this overlay must remain live.
void shelterB1StoreroomRoomVisualEffectsMoteTask(Task* task);

/// Runs an expanding tinted halo, shrinking ring and fading star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. The unsigned low half of `spawnArg1` is a nonzero expansion
/// duration in active ticks; the signed high half selects tint row 0..2.
/// Initialization attaches at the saved parent-local offset and replaces the
/// argument with its countdown. State 3 requests release. Nonzero room effect
/// control pauses every phase; four or above cancels it. Completion or
/// cancellation releases the work and task. The borrowed parent, effect
/// controller and this overlay must remain live.
void shelterB1StoreroomRoomVisualEffectsHaloTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Uses the halo-effect drawers and refreshes transient light slot 2.
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// or cancellation releases the work and task. Coordinate ancestors, the
/// effect controller and this overlay must remain live.
void shelterB1StoreroomRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty independent descending motes around a turning, rising spawn offset.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. Active ages
/// 1..20 emit at radius about 768 and Y = -128 * age in parent-axis game units.
/// Each mote descends eight units per active tick and outlives the emitter,
/// whose work and task retire at age 21. Nonzero room effect control pauses;
/// four or above cancels and releases it. Requires the installed mote effect,
/// live coordinate ancestors, controller and room overlay through teardown.
void shelterB1StoreroomRoomVisualEffectsSparkEmitterTask(Task* task);

/// Binds the room's effect IDs once and draws its active view's light glows each frame.
///
/// State 0 selects this overlay's effect callbacks and advances to state 1.
/// Every state draws in mapped views 2..7; other views emit nothing. The task
/// owns no additional work and ignores spawn arguments and room effect control.
/// Requires this overlay to remain loaded, a composed view, initialized scratch
/// storage and room in the current frame's packet arena and ordering table.
void shelterB1StoreroomDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H
