#ifndef INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H
#define INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_forest_zone_80182968[13];

// neo_ark_forest_zone
extern WorldCollisionRoomResources D_neo_ark_forest_zone_801820A4[];

extern WorldCoordRoomLighting D_neo_ark_forest_zone_801820B4[];

extern u8* D_neo_ark_forest_zone_801820BC[];

extern ViewCount D_neo_ark_forest_zone_801820C0[];

extern DirectionWarpEntry D_neo_ark_forest_zone_801820C4[];

extern ViewCamera D_neo_ark_forest_zone_80182298[];

extern SpriteView D_neo_ark_forest_zone_80182594[];

extern WorldCollisionSurfaceProperties* D_neo_ark_forest_zone_80182CE4[];

/// Selects the forest zone's enemy effects and keeps ambient effects enabled.
///
/// State 0 installs this overlay's flash, twin-trail and spark-burst IDs, then
/// advances to state 1. Every tick enables the ambient-effect gate, including
/// footstep dust. Requires a live `gRoomEffectState`; runs until external teardown.
void neoArkForestZoneConfigureEffectsTask(Task* task);

/// Runs the forest zone's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// the task; control 4 or above, state 3, or completion releases the work and task.
void neoArkForestZoneRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws the forest zone's fading twin trail.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its parent coordinate must remain live until teardown.
/// `Task::work` starts null and owns two eight-coordinate world-space histories.
/// The signed 16-bit age advances only while room effect control is below 2,
/// including initialization; allocation failure resets it to zero for retry.
/// `spawnArg1.value` is the completion age (2..32767); zero disables automatic
/// completion. Teardown releases both the history and effect allocations.
void neoArkForestZoneRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the forest zone's impact flash with smoke or fading rings and bouncing sparks.
///
/// Requires a counted coordinate-body effect with zero-aged, owned `EffectWork`
/// in `spawnArg2.pointer` and initial state zero. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and sparks. Active age 7 enters release and the next
/// active tick frees work and task. Nonzero room control below 4 pauses; control
/// 4 or above cancels. Spawned effects have independent task lifetimes.
void neoArkForestZoneRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs one tumbling forest leaf through falling, a stationary hold and fading.
///
/// Requires a coordinate body and counted, cleared `EffectWork` owned through
/// `spawnArg2.pointer`, as `effectSpawn` supplies. Motion uses parent-coordinate
/// game units and 4096 angle units per turn. Falling stops when parent-space Y
/// becomes positive. Completion releases the work and task; room effect pause
/// and cancellation controls do not gate this task.
void neoArkForestZoneLeafFallTask(Task* task);

/// Runs the forest zone's room-message task and first-visit encounter setup.
///
/// Requires a live task in state 0..3: initialize the receiver, ambience and
/// pool-B controller; pause that pool for the first-visit scene; idle; then
/// teardown. The room overlay must remain loaded through dispatch.
void neoArkForestZoneRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H
