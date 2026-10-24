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

/// Actor layout variants for the Acropolis hallway.
extern AreaVariant gAkropolisHallwayAreaVariants[13];

extern TmdSource gAkropolisHallwayBlueKeyPickupModel;

/// Collision resources for hallway room 1: mesh, view boundaries and action quads.
extern WorldCollisionRoomResources gAkropolisHallwayRoomCollisionResources[1];

/// Hallway room 1's logical-view map, borrowing gameplay's identity map.
extern u8* gAkropolisHallwayViewMaps[1];

/// Hallway room 1's five logical views, limiting reverse map searches.
extern ViewCount gAkropolisHallwayViewCounts[1];

/// Hallway room 1's three point lights and default ambient minima.
extern WorldCoordRoomLighting gAkropolisHallwayRoomLightingTable[1];

/// Four hallway warp endpoints, indexed by the 1-based warp ID minus one.
extern DirectionWarpEntry gAkropolisHallwayWarpEntries[4];

/// Hallway mapped views 1..5, selecting decoded backgrounds without source sprites.
extern SpriteView gAkropolisHallwaySpriteViews[5];

/// Hallway cameras for mapped views 1..5, indexed by mapped view minus one.
extern ViewCamera gAkropolisHallwayViewCameras[5];

/// Hallway surface responses for collision classes 0..7, with no terminator.
extern WorldCollisionSurfaceProperties* gAkropolisHallwaySurfacePropertiesByClass[8];

/// Inert task callback for the hallway's effect-bank-6 slot 0x84.
void akropolisHallwayEffectControlTask84(Task* unusedTask);

/// Runs the hallway's room-message receiver.
void akropolisHallwayRoomTask(Task* task);

/// Shows the hallway Blue Key pickup only in mapped view 5 until collected.
void akropolisHallwayBlueKeyPickupTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_HALLWAY_H
