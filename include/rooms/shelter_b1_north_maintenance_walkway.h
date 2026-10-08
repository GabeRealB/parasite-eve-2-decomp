#ifndef INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_north_maintenance_walkway_80185A38[12];

// shelter_b1_north_maintenance_walkway
extern u8* D_shelter_b1_north_maintenance_walkway_80184B80[];

extern ViewCount D_shelter_b1_north_maintenance_walkway_80184B84[];

extern DirectionWarpEntry D_shelter_b1_north_maintenance_walkway_80184B88[];

extern WorldCollisionGrid D_shelter_b1_north_maintenance_walkway_80184F40;

extern ViewCamera D_shelter_b1_north_maintenance_walkway_80184F64[];

extern SpriteView D_shelter_b1_north_maintenance_walkway_801853AC[];

extern WorldCoordRoomLights D_shelter_b1_north_maintenance_walkway_801855D4;

extern WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_801855EC[];

extern WorldCollisionOccluder D_shelter_b1_north_maintenance_walkway_801857B4[];

extern WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_80185A98[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_north_maintenance_walkway_80185B4C[];

/// Runs the walkway's room-message receiver and variant-dependent scene setup.
///
/// Requires state 0..2 for its three-entry dispatch table. State 0 registers
/// the receiver and initializes scenes/sprites, state 1 idles and state 2 kills
/// it. Keep the room and Shelter map overlays loaded while the task is live.
void shelterB1NorthMaintenanceWalkwayRoomTask(Task* task);

/// Runs a vertically drifting animated mote until it fades.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1` packs half-extent in bits 0..11, palette in
/// bits 12..15, unsigned speed in bits 16..23 and signed lifetime in bits
/// 24..31. Extent and speed use parent-coordinate units; lifetime uses active
/// ticks. Motion bits 0..1 overlap the extent: either selects steady motion,
/// with bit 1 selecting upward motion; neither selects a brightening rise
/// with added random speed. Initialization draws nothing; later ticks draw
/// on odd ages. Nonzero room effect control pauses it; four or above cancels it.
/// Completion or cancellation releases work and task. Coordinate ancestors,
/// the effect controller and this room overlay must remain live.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task);

/// Charges a pink flash, tints the screen at its peak, then fades as a star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.value` is a positive charge duration in active
/// ticks, consumed as a countdown after initialization. State 3 requests early
/// release. Nonzero room effect control pauses every phase, including release;
/// four or above cancels it. Completion or cancellation releases work and task.
/// Coordinate ancestors, the effect controller and this room overlay must
/// remain live; drawing requires the current frame's packet arena and scratch stack.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue twin trail.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate and `Task::work` initially NULL.
/// Owns two eight-coordinate histories in `Task::work`; allocation failure
/// retries initialization. Endpoints use this room's two fixed local offsets;
/// snapshots retain their world positions as the parent moves. `spawnArg1.value`
/// zero leaves lifetime to parent teardown; values 2..32767 release at that
/// age in active ticks. Control values 2 and above pause updates and expiry;
/// control 1 still records and draws. Effect teardown frees both work allocations.
/// The parent, effect controller and this room overlay must remain live until teardown.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the walkway's impact flash with smoke puffs or fading rings and bouncing sparks.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`, starting in state 0. Nonzero
/// `spawnArg1.value` selects smoke; zero selects rings and two sparks. Enters
/// release at active age 7 and frees work and task on the next active tick.
/// Spawned effects have independent lifetimes. Nonzero room effect control
/// pauses updates; four or above cancels. Coordinate ancestors, the effect
/// controller and this room overlay must remain live until teardown.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the walkway's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`; `spawnArg1.value` selects tint 0 or 1.
/// Start in `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH`; its owner can request
/// FLICKER, RELEASE or CANCEL through the corresponding state constants.
/// Borrows the work's parent coordinate and saved local offset. Growth emits
/// flying sparks from live player model parts 3..18 and adopts them for teardown;
/// the flying-spark selector must be installed. Nonzero room effect control
/// pauses updates; four or above cancels. Release/cancel frees work and child
/// tasks. The anchor, player, effect controller and room overlay remain live.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task);

/// Flies an animated spark along a fixed step toward its initial target position.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.pointer` borrows a target `GfxCoord` whose cached
/// transform, like the spark's, must be composed into the same view space
/// on the first active tick.
/// That tick fixes the step at 204/4096 of the initial displacement in parent
/// axes, narrowing components to s16; the target is never sampled again.
/// Later active ticks move by that step and draw on odd ages; age 20 releases
/// work and task. Nonzero room effect control pauses it; four or above cancels
/// it. Coordinate ancestors, the effect controller and this room overlay must
/// remain live; the target is needed only for initialization.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Uses the flying-effect drawers and refreshes transient light slot 2.
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// or cancellation releases work and task. Coordinate ancestors, the effect
/// controller and this room overlay must remain live.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs an expanding tinted halo, shrinking ring and fading star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. The unsigned low half of `spawnArg1` is a nonzero expansion
/// duration in active ticks; the signed high half selects tint row 0..2.
/// Initialization attaches at the saved parent-local offset and replaces the
/// argument with its countdown. State 3 requests early release. Nonzero room
/// effect control pauses every phase; four or above cancels it. Completion or
/// cancellation releases work and task. The borrowed parent, effect controller
/// and this room overlay must remain live.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Uses the halo-effect drawers and refreshes transient light slot 2.
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// or cancellation releases work and task. Coordinate ancestors, the effect
/// controller and this room overlay must remain live.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty motes at rotating, successively higher offsets around its coordinate.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`; `spawnArg1` is unused. Each active
/// age 1..20 emits an independent mote that descends eight parent-axis units
/// per tick; age 21 releases the emitter's work and task. Requires the room's
/// mote selector to be installed. Nonzero room effect control pauses updates;
/// four or above cancels. Coordinate ancestors and the effect controller stay
/// live through teardown; this room overlay stays loaded while emitted motes run.
void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask(Task* task);

/// Binds the room's effect tasks once and draws the active view's lamp glows each frame.
///
/// Task state 0 selects the room effect IDs and advances to state 1; every state
/// draws glows in views 2..6, while other views emit none. Requires this room
/// overlay to remain loaded, composed view matrices, the glow drawers' scratch
/// stack capacity, and room in the current frame's packet arena and ordering table.
void shelterB1NorthMaintenanceWalkwayDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
