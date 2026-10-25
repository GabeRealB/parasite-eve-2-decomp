#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_EAST_ELEVATOR_HALL_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_EAST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// The hall's room states, views and warps.
enum {
    AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT = 1,
    AKROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT = 7,
    AKROPOLIS_EAST_ELEVATOR_HALL_WARP_COUNT = 1,
};

/// Placement variants: 1 has the wounded SWAT member.
extern AreaVariant gAkropolisEastElevatorHallAreaVariants[3];

/// Collision of the hall's one room state.
extern WorldCollisionRoomResources gAkropolisEastElevatorHallRoomCollisionResources[AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// View map of the hall's one room state: every view is its own camera.
extern u8* gAkropolisEastElevatorHallViewMaps[AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Number of views: 7.
extern ViewCount gAkropolisEastElevatorHallViewCounts[AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Lighting of the hall's one room state: four point lights.
extern WorldCoordRoomLighting gAkropolisEastElevatorHallRoomLightingTable[AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Arrival point of warp 1, the elevator door from the square.
extern DirectionWarpEntry gAkropolisEastElevatorHallWarpEntries[AKROPOLIS_EAST_ELEVATOR_HALL_WARP_COUNT];

/// Occlusion sprites of each view: pieces of the background drawn over the models behind them.
extern SpriteView gAkropolisEastElevatorHallSpriteViews[AKROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT];

/// Cameras of each view.
extern ViewCamera gAkropolisEastElevatorHallViewCameras[AKROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT];

/// Surface properties by collision class.
extern WorldCollisionSurfaceProperties* gAkropolisEastElevatorHallSurfacePropertiesByClass[8];

/// The room's effects: player reflections, and the six red beacons in view 2.
void akropolisEastElevatorHallReflectionsAndBeaconsTask(Task* task);

/// Draws one frame of a pulsing red beacon.
void akropolisEastElevatorHallRedBeaconTask(Task* task);

/// The player's reflection on the floor (0) or the room's mirror plane (1).
void akropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// Draws one frame of a gray point of light.
void akropolisEastElevatorHallPointTileTask(Task* task);

/// The hall's room task: installs its messages and shows the wounded SWAT member.
void akropolisEastElevatorHallRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_EAST_ELEVATOR_HALL_H
