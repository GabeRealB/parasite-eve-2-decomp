#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision of the hall's one room state.
extern WorldCollisionRoomResources gAkropolisWestElevatorHallCollisionRoomResources[1];

/// View map of the hall's one room state: every view is its own camera.
extern u8* gAkropolisWestElevatorHallViewMaps[1];

/// Number of views: 5.
extern ViewCount gAkropolisWestElevatorHallViewCounts[1];

/// Lighting of the hall's one room state.
extern WorldCoordRoomLighting gAkropolisWestElevatorHallRoomLightingTable[1];

/// Arrival points by warp: 1 the elevator, which the arrival scene comes in by, and 2 the square's door.
extern DirectionWarpEntry gAkropolisWestElevatorHallWarpEntries[2];

/// Occlusion sprites of each view: pieces of the background drawn over the models behind them.
extern SpriteView gAkropolisWestElevatorHallSpriteViews[5];

/// Cameras of each view.
extern ViewCamera gAkropolisWestElevatorHallViewCameras[5];

/// Surface properties by collision class.
extern WorldCollisionSurfaceProperties* gAkropolisWestElevatorHallSurfacePropertiesByClass[8];

/// Draws one frame of a pulsing red beacon.
void akropolisWestElevatorHallRedBeaconTask(Task* task);

/// Shimmers an 82-row, 120-pixel strip of the frame for one frame, each row shifted along a wave. Nothing spawns it.
void akropolisWestElevatorHallScanlineDistortionTask(Task* task);

/// Draws one frame of the elevator bay's light.
void akropolisWestElevatorHallLightGlowTask(Task* task);

/// Lights the elevator bay as the elevator arrives: ramps its palette from black over two frames, and keeps it lit
/// until the view leaves the bay.
void akropolisWestElevatorHallBayLightingTask(Task* task);

/// The player's reflection on the floor (0) or the room's mirror plane (1).
void akropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// The room's effects: player reflections, the three red beacons in view 2 and the bay's light in view 5.
void akropolisWestElevatorHallViewEffectsTask(Task* task);

/// The hall's room task: installs its messages, spawns the elevator door's two leaves and plays the arrival scene
/// (the elevator arrives and Aya steps out) the first time she comes in by it.
void akropolisWestElevatorHallRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H
