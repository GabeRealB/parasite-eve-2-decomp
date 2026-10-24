#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_PATIO_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_PATIO_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// The patio's placement variants, room states, views, warps and surface classes.
enum {
    AKROPOLIS_PATIO_VARIANT_SLOT_COUNT  = 12,
    AKROPOLIS_PATIO_ROOM_COUNT          = 3,
    AKROPOLIS_PATIO_VIEW_COUNT          = 19,
    AKROPOLIS_PATIO_WARP_COUNT          = 4,
    AKROPOLIS_PATIO_SURFACE_CLASS_COUNT = 8,
};

/// Placement variants: 1..5 and 10 have placements, the others none.
extern AreaVariant gAkropolisPatioAreaVariants[AKROPOLIS_PATIO_VARIANT_SLOT_COUNT];

/// Collision of the patio's three room states: one grid, with room state 1's action triggers or those of 2 and 3.
extern WorldCollisionRoomResources gAkropolisPatioRoomCollisionResources[AKROPOLIS_PATIO_ROOM_COUNT];

/// View maps of the room states: from state 2, view 4 shows camera 17; in state 3, view 7 also shows 13.
extern u8* gAkropolisPatioViewMaps[AKROPOLIS_PATIO_ROOM_COUNT];

/// Number of views of each room state: 19.
extern ViewCount gAkropolisPatioViewCounts[AKROPOLIS_PATIO_ROOM_COUNT];

/// Lighting of the room states: one set of 24 point lights.
extern WorldCoordRoomLighting gAkropolisPatioRoomLightingTable[AKROPOLIS_PATIO_ROOM_COUNT];

/// Arrival points by warp: 1 and 4 the stairs from the square, 2 the fountain door, 3 the cafeteria door.
extern DirectionWarpEntry gAkropolisPatioWarpEntries[AKROPOLIS_PATIO_WARP_COUNT];

/// Occlusion sprites of each view: pieces of the background drawn over the models behind them.
extern SpriteView gAkropolisPatioSpriteViews[AKROPOLIS_PATIO_VIEW_COUNT];

/// Cameras of each view.
extern ViewCamera gAkropolisPatioViewCameras[AKROPOLIS_PATIO_VIEW_COUNT];

/// Surface properties by collision class; class 4 has footsteps. The last entry is a NULL never read.
extern WorldCollisionSurfaceProperties* gAkropolisPatioSurfacePropertiesByClass[AKROPOLIS_PATIO_SURFACE_CLASS_COUNT + 1];

/// Spawns the patio's light glows (three large, four in the ceiling, seven on the walls) and three motes around
/// each large light.
void akropolisPatioSpawnLightEffectsTask(Task* task);

/// Draws one frame of a light's flickering glow, in the views that show it.
void akropolisPatioLightGlowTask(Task* task);

/// Moves and draws one frame of a mote drifting around its light, in the views that show it.
void akropolisPatioLightMoteTask(Task* task);

/// The patio's room task: installs its messages and sets its actors up for the story so far.
void akropolisPatioRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_PATIO_H
