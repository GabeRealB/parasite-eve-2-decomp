#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_ROOF_GARDEN_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_ROOF_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Actor layout variants of the roof garden.
extern AreaVariant gAkropolisRoofGardenAreaVariants[12];

extern TmdSource gAkropolisRoofGardenArmoryCardkeyPickupModel;

/// Collision resources for roof-garden room 1, indexed by room minus one.
extern WorldCollisionRoomResources gAkropolisRoofGardenCollisionRoomResources[1];

/// Number of logical views in the roof garden's single room.
enum { AKROPOLIS_ROOF_GARDEN_VIEW_COUNT = 7 };

/// Logical-to-mapped view directory for roof-garden room 1.
extern u8* gAkropolisRoofGardenViewMaps[1];

/// Reverse-lookup view count for roof-garden room 1, in logical-view elements.
extern ViewCount gAkropolisRoofGardenViewCounts[1];

/// Model-shading lights and ambient policy for roof-garden room 1.
extern WorldCoordRoomLighting gAkropolisRoofGardenRoomLightingTable[1];

/// Two roof-garden warp endpoints, indexed by the 1-based warp ID minus one.
extern DirectionWarpEntry gAkropolisRoofGardenWarpEntries[2];

/// Sprite overlays for mapped roof-garden views 1..7, indexed by view minus one.
extern SpriteView gAkropolisRoofGardenSpriteViews[AKROPOLIS_ROOF_GARDEN_VIEW_COUNT];

/// Camera transforms and pixel projection distances for the roof garden.
extern ViewCamera gAkropolisRoofGardenViewCameras[8];

/// Collision and footstep policies indexed by roof-garden surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisRoofGardenSurfacePropertiesByClass[8];

/// Places the roof garden's ten light glows and emits its view-dependent red flares.
void akropolisRoofGardenAmbientEffectsTask(Task* task);

/// Draws a flickering light sprite in the roof garden views that see it.
void akropolisRoofGardenLightGlowTask(Task* task);

/// Draws one frame of a pulsing red or green flare, then retires the effect task.
void akropolisRoofGardenFlareTask(Task* task);

/// Advances a tumbling leaf through its fall, stationary hold and fade.
void akropolisRoofGardenLeafFallTask(Task* task);

/// Dispatches the roof-garden room task's setup, per-frame scene check and teardown.
void akropolisRoofGardenRoomTask(Task* task);

/// Selects the roof-garden pickup model's visibility from its saved state and view.
void akropolisRoofGardenPickupModelTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_ROOF_GARDEN_H
