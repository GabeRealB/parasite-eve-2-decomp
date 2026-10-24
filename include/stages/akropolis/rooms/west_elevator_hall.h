#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources for the west elevator hall's single room.
extern WorldCollisionRoomResources gAkropolisWestElevatorHallCollisionRoomResources[1];

/// Logical-view map directory for the hall's single room.
extern u8* gAkropolisWestElevatorHallViewMaps[1];

/// Number of logical views in room 1: five, matching the sprite and camera arrays.
extern ViewCount gAkropolisWestElevatorHallViewCounts[1];

/// Room-1 model lighting, with the hall's point lights and default ambient minima.
extern WorldCoordRoomLighting gAkropolisWestElevatorHallRoomLightingTable[1];

/// Hall warp endpoints 1 (elevator arrival) and 2 (the square's doorway).
extern DirectionWarpEntry gAkropolisWestElevatorHallWarpEntries[2];

/// Foreground sprite resources for mapped views 1..5, indexed by view minus one.
extern SpriteView gAkropolisWestElevatorHallSpriteViews[5];

/// Cameras for mapped hall views 1..5, indexed by view minus one.
extern ViewCamera gAkropolisWestElevatorHallViewCameras[5];

/// Hall collision policies and footstep cues indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisWestElevatorHallSurfacePropertiesByClass[8];

/// Draws one frame of a pulsing additive red diamond, then ends the counted effect.
void akropolisWestElevatorHallRedBeaconTask(Task* task);

/// Distorts an 82-row, 120-pixel strip of the drawing framebuffer for one frame.
void akropolisWestElevatorHallScanlineDistortionTask(Task* task);

/// Draws one frame of the hall's additive light billboard, then retires its effect.
void akropolisWestElevatorHallLightGlowTask(Task* task);

/// Lights the elevator bay's palette over two updates and holds it in view 5.
void akropolisWestElevatorHallBayLightingTask(Task* task);

/// Runs this room's player-model floor or mirror-plane reflection.
void akropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// Registers the hall's room effects and emits lights for the current view.
void akropolisWestElevatorHallViewEffectsTask(Task* task);

/// Runs the west elevator hall's room messages, door leaves and arrival event.
void akropolisWestElevatorHallRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_WEST_ELEVATOR_HALL_H
