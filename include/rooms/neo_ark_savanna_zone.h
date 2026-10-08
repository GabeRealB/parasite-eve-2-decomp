#ifndef INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
#define INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_savanna_zone_80180864[13];

// neo_ark_savanna_zone
extern WorldCollisionRoomResources D_neo_ark_savanna_zone_8017F9E4[];

extern WorldCoordRoomLighting D_neo_ark_savanna_zone_8017F9F4[];

extern u8* D_neo_ark_savanna_zone_8017F9FC[];

extern ViewCount D_neo_ark_savanna_zone_8017FA00[];

extern DirectionWarpEntry D_neo_ark_savanna_zone_8017FA04[];

extern ViewCamera D_neo_ark_savanna_zone_8017FBF4[];

extern SpriteView D_neo_ark_savanna_zone_801803F4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_savanna_zone_80180968[];

/// Installs the savanna zone's enemy effects and enables ambient effects once.
///
/// State 0 selects this overlay's flash, twin-trail and spark-burst effect IDs
/// and enables room effects, including footstep dust, then enters idle state 1.
/// Requires a live `gRoomEffectState`; later ticks leave the settings alone.
void neoArkSavannaZoneConfigureEffectsTask(Task* task);

/// Runs the savanna zone's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, as `effectSpawn` supplies. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown after initialization.
/// Nonzero room effect control pauses without drawing; control 4 or above
/// cancels it. While running, state 3 or fade completion releases it. Release
/// frees the work, body and task. The room overlay and effect controller must
/// remain live, with scratch and frame packet space.
void neoArkSavannaZoneRoomVisualEffectsFlashTask(Task* task);

/// Draws the savanna zone's fading beam between two offsets on a moving parent.
///
/// Requires a coordinate body, counted, owned `EffectWork` in `spawnArg2.pointer`,
/// initial state 0 and null `Task::work`. The borrowed parent coordinate, room
/// overlay and effect controller must remain live. Allocates eight world-space
/// snapshots per endpoint in `Task::work`; allocation failure resets age for retry.
/// `spawnArg1.value` is the release age in active ticks (2..32767); zero leaves
/// lifetime to external teardown. Initialization increments age without drawing
/// or checking expiry. Control values below 2 advance the trail; 2 and above
/// freeze it without drawing or cancellation. Teardown frees the histories,
/// effect work, body and task. Drawing needs scratch and frame packet space.
void neoArkSavannaZoneRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_savanna_zone_8017ED58(Task* task);

/// Runs the Neo Ark Savanna Zone room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages, remain idle between messages, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkSavannaZoneRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
