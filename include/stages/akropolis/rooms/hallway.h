#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_HALLWAY_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_HALLWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Placement variants of the hallway.
extern AreaVariant gAkropolisHallwayAreaVariants[13];

/// The blue key's model, which the Akropolis stage's pickup table places.
extern TmdSource gAkropolisHallwayBlueKeyPickupModel;

/// Collision of the hallway's one room state.
extern WorldCollisionRoomResources gAkropolisHallwayRoomCollisionResources[1];

/// View map of the hallway's one room state: gameplay's, where every view is its own camera.
extern u8* gAkropolisHallwayViewMaps[1];

/// Number of views: 5.
extern ViewCount gAkropolisHallwayViewCounts[1];

/// Lighting of the hallway's one room state: three point lights.
extern WorldCoordRoomLighting gAkropolisHallwayRoomLightingTable[1];

/// Arrival points by warp.
extern DirectionWarpEntry gAkropolisHallwayWarpEntries[4];

/// Occlusion sprites of each view: none.
extern SpriteView gAkropolisHallwaySpriteViews[5];

/// Cameras of each view.
extern ViewCamera gAkropolisHallwayViewCameras[5];

/// Surface properties by collision class.
extern WorldCollisionSurfaceProperties* gAkropolisHallwaySurfacePropertiesByClass[8];

/// The hallway's room effect task: it has no effects.
void akropolisHallwayRoomEffectTask(Task* unusedTask);

/// The hallway's room task: installs its messages.
void akropolisHallwayRoomTask(Task* task);

/// The blue key in its box: drawn in view 5, the box's close view, until it is taken ("A key is hanging here.").
void akropolisHallwayBlueKeyPickupTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_HALLWAY_H
