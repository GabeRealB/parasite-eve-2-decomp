#ifndef INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H
#define INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_forked_road_801831C4[24];

// acropolis_forked_road
extern WorldCollisionRoomResources D_acropolis_forked_road_80182214[];

/// Logical-view maps for forked-road rooms 1..3, indexed by room minus one.
///
/// Each byte at logical view minus one is a 1-based camera/image/sprite index.
/// Room 1 borrows `gViewIdentityMap` for ten views; rooms 2 and 3 each search
/// nine entries of their own twelve-byte maps. Maps have no terminator; their
/// trailing zero entries must not select resources. Keep this overlay loaded
/// while its maps are borrowed by the stage directory.
extern u8* gAcropolisForkedRoadViewMaps[3];

extern ViewCount D_acropolis_forked_road_80182268[];

/// Model-lighting resources for forked-road rooms 1..3, indexed by room minus one.
///
/// All entries borrow the same mutable collection of 24 point lights, with no
/// directional or cone lights. NULL ambient tables select gameplay's default
/// ambient minima. Keep this overlay loaded while its resources are borrowed.
extern WorldCoordRoomLighting gAcropolisForkedRoadRoomLightingTable[3];

extern DirectionWarpEntry D_acropolis_forked_road_80182288[];

extern SpriteView D_acropolis_forked_road_801844E0[];

extern ViewCamera D_acropolis_forked_road_80184E88[];

/// Collision and footstep policies indexed by the forked road's surface class 0..7.
///
/// Every class blocks probes and applies pushback. Classes 2 and 3 suppress
/// weapon impact responses; the others allow them. Only class 4 supplies
/// footstep cues. There is no terminator, and the borrowed records and cues
/// remain valid only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gAcropolisForkedRoadSurfacePropertiesByClass[8];

/// Spawns fourteen wall lamps and publishes this room's flash, trail and burst IDs.
///
/// Bank-6 slot 0x85 requires a live coordinate body in state 0. Spawns one lamp
/// per room offset, packing its index, sprite cell and world-size scale into
/// the argument. Advances to state 1 even if a spawn fails; later ticks do
/// nothing. The controller remains live until external teardown. Its coordinate
/// hierarchy and this overlay must remain loaded while the lamps use them.
void acropolisForkedRoadInitializeRoomEffectsTask(Task* task);

/// Draws a flickering, camera-facing wall lamp in the logical views that see it.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. Initially `spawnArg1.value` packs a lamp index 0..13 in bits
/// 0..3, a sprite cell 0..2 in bits 8..9, and a world-size scale in bits 16..27
/// (zero selects 640). After the first visible tick only the lamp index remains;
/// `angle` holds the cell and `period` its resting grey level. The active logical
/// view must be valid and 1-based. Control values 4 and above suppress drawing
/// without releasing the task. A permitted view consumes one `POLY_FT4` even
/// when its depth is below 17; requires frame-arena and scratch-stack capacity.
void acropolisForkedRoadWallLampTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`; `spawnArg1.value` is a positive charge duration in callback
/// ticks, consumed as a countdown after initialization. Nonzero room effect
/// control pauses the task, and values 4 and above cancel it. State 3 requests
/// release; completion or cancellation releases the counted work and task.
void acropolisForkedRoadRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading twin-trail beam.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate, and `Task::work` initially NULL.
/// Borrows the parent for its lifetime and owns two eight-coordinate histories
/// in `work`; allocation failure retries initialization. `spawnArg1.value` is
/// zero for an unlimited lifetime, or 2..32767 for an age in active ticks at
/// which to release. Room effect control values 2 and above pause updates.
/// Normal effect teardown releases both the counted work and history block.
void acropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
///
/// Bank-6 slot 0x292 requires a coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// two independent sparks and fading rings. Enters release at active age seven
/// and frees the counted work/task on the next active tick. Nonzero room effect
/// control pauses below four and cancels at four or above. Keep the room overlay
/// and the borrowed coordinate hierarchy loaded until effect teardown.
void acropolisForkedRoadRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs a tumbling leaf through its fall, stationary hold and brightness fade.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. Random drift and X/Z tumble advance
/// each tick until local Y becomes positive. The held square has half-size 32
/// coordinate units; its final fade releases the counted work and task.
void acropolisForkedRoadLeafFallTask(Task* task);

/// Runs the forked road's room messages and return-arrival scene gate.
///
/// State 0 registers the receiver; state 1 starts the return scene once for
/// arrival warp 2; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisForkedRoadRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H
