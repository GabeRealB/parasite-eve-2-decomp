#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_SQUARE_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_SQUARE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Area updates after the first call: the patio and cafeteria move to variant 3,
/// the MIST parking lot to variant 2.
extern AreaApplyRec gAkropolisSquarePostCutsceneAreaUpdates[4];

/// Placement variants: 1 has the grinning stranger, 2 is empty after the telephone scene.
extern AreaVariant gAkropolisSquareAreaVariants[3];

/// Collision of the square's one room state.
extern WorldCollisionRoomResources gAkropolisSquareRoomCollisionResources[1];

/// View map of the square's one room state: every view is its own camera.
extern u8* gAkropolisSquareViewMaps[1];

/// Number of views: 15.
extern ViewCount gAkropolisSquareViewCounts[1];

/// Lighting of the square's one room state.
extern WorldCoordRoomLighting gAkropolisSquareRoomLightingTable[1];

/// Arrival points by warp: 1 and 7 the west elevator, 2 the east elevator,
/// 3..6 the patio and the forked road.
extern DirectionWarpEntry gAkropolisSquareWarpEntries[7];

/// Occlusion sprites of each view: pieces of the background drawn over the models behind them.
extern SpriteView gAkropolisSquareSpriteViews[15];

/// Cameras of each view.
extern ViewCamera gAkropolisSquareViewCameras[15];

/// Surface properties by collision class.
extern WorldCollisionSurfaceProperties* gAkropolisSquareSurfacePropertiesByClass[8];

/// Starts the telephone scene the first time the grinning stranger is passed.
/// Returns 0 when it starts, 1 when it has already played.
s32 akropolisSquareStartTelephoneScene(s32 unused);

/// The room's effects: player reflections and the telephone's light.
void akropolisSquareRoomEffectTask(Task* task);

/// Draws one frame of the telephone's pulsing light.
void akropolisSquareTelephoneLightTask(Task* task);

/// The telephone's save menu.
void akropolisSquareTelephoneMenuTask(Task* task);

/// The player's reflection on the floor (0) or the room's mirror plane (1).
void akropolisSquarePlayerReflectionTask(Task* reflectionTask);

/// The square's room task: installs its messages and starts the arrival scene.
void akropolisSquareRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_SQUARE_H
