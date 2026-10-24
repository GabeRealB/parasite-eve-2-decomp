#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_FORKED_ROAD_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_FORKED_ROAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Actor layout variants of the forked road.
extern AreaVariant gAkropolisForkedRoadAreaVariants[24];

/// Collision resources for forked-road rooms 1..3, indexed by room minus one.
extern WorldCollisionRoomResources gAkropolisForkedRoadRoomCollisionResources[3];

/// Logical-view maps for forked-road rooms 1..3, indexed by room minus one.
extern u8* gAkropolisForkedRoadViewMaps[3];

/// Logical-view search counts for forked-road rooms 1..3, indexed by room minus one.
extern ViewCount gAkropolisForkedRoadViewCounts[3];

/// Model-lighting resources for forked-road rooms 1..3, indexed by room minus one.
extern WorldCoordRoomLighting gAkropolisForkedRoadRoomLightingTable[3];

/// Arrival placements and departure policies for forked-road warp slots 1..5.
extern DirectionWarpEntry gAkropolisForkedRoadWarpEntries[5];

/// Sprite resources for twelve mapped forked-road views, indexed by mapped view minus one.
extern SpriteView gAkropolisForkedRoadSpriteViews[12];

/// Cameras for twelve mapped forked-road views, indexed by mapped view minus one.
extern ViewCamera gAkropolisForkedRoadViewCameras[12];

/// Collision and footstep policies indexed by the forked road's surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisForkedRoadSurfacePropertiesByClass[8];

/// Spawns fourteen wall lamps and publishes this room's flash, trail and burst IDs.
void akropolisForkedRoadInitializeRoomEffectsTask(Task* task);

/// Draws a flickering, camera-facing wall lamp in the logical views that see it.
void akropolisForkedRoadWallLampTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
void akropolisForkedRoadRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading twin-trail beam.
void akropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
void akropolisForkedRoadRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs a tumbling leaf through its fall, stationary hold and brightness fade.
void akropolisForkedRoadLeafFallTask(Task* task);

/// Runs the forked road's room messages and return-arrival scene gate.
void akropolisForkedRoadRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_FORKED_ROAD_H
