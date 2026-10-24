#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_R18_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_R18_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Placement and resource layouts of the briefing room.
extern AreaVariant gMistR18AreaVariants[13];

/// Collision resources for the single briefing room, indexed by room minus one.
extern WorldCollisionRoomResources gMistR18CollisionRoomResources[1];

/// Logical-view map directory for the single briefing room.
extern u8* gMistR18ViewMaps[1];

/// Reverse-search limit for room 1's ten logical views, with a retained zero halfword.
extern ViewCount gMistR18ViewCounts[2];

/// Room-1 model lighting, selecting five point lights and default ambient minima.
extern WorldCoordRoomLighting gMistR18RoomLightingTable[1];

/// Sole briefing-area warp endpoint, indexed by warp minus one (warp 1 only).
extern DirectionWarpEntry gMistR18WarpEntries[1];

/// Cameras for mapped briefing views 1..10, indexed by mapped view minus one.
extern ViewCamera gMistR18ViewCameras[10];

/// Foreground sprite resources for mapped views 1..10, indexed by view minus one.
extern SpriteView gMistR18SpriteViews[10];

/// Collision policies and footstep requests indexed by room-local surface class 0..7.
extern WorldCollisionSurfaceProperties* gMistR18SurfacePropertiesByClass[8];

/// Runs the MIST briefing and its key-item menu before departure to Dryfield.
void mistR18BriefingTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_R18_H
