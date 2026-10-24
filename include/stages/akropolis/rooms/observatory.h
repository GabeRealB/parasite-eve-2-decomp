#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_OBSERVATORY_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the observatory's room, mapped-view and surface-class tables.
enum {
    AKROPOLIS_OBSERVATORY_ROOM_COUNT          = 2,
    AKROPOLIS_OBSERVATORY_VIEW_COUNT          = 8,
    AKROPOLIS_OBSERVATORY_SURFACE_CLASS_COUNT = 8,
    AKROPOLIS_OBSERVATORY_WARP_COUNT          = 4,
};

/// Actor layout variants of the observatory.
extern AreaVariant gAkropolisObservatoryAreaVariants[19];

/// Collision resources for rooms 1 and 2, indexed by room minus one.
extern WorldCollisionRoomResources gAkropolisObservatoryRoomCollisionResources[AKROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Logical-to-mapped view indices for rooms 1 and 2, indexed by room minus one.
extern u8* gAkropolisObservatoryViewMaps[AKROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Signed logical-view counts for rooms 1 and 2, indexed by room minus one.
extern ViewCount gAkropolisObservatoryViewCounts[AKROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Lighting descriptors for rooms 1 and 2, indexed by room minus one.
extern WorldCoordRoomLighting gAkropolisObservatoryRoomLightingTable[AKROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Four arrival/departure endpoints, indexed by 1-based warp ID minus one.
extern DirectionWarpEntry gAkropolisObservatoryWarpEntries[AKROPOLIS_OBSERVATORY_WARP_COUNT];

/// Foreground sprite resources for mapped views 1..8, indexed by view minus one.
extern SpriteView gAkropolisObservatorySpriteViews[AKROPOLIS_OBSERVATORY_VIEW_COUNT];

/// Camera transforms and projection distances for mapped views 1..8.
extern ViewCamera gAkropolisObservatoryViewCameras[AKROPOLIS_OBSERVATORY_VIEW_COUNT];

/// Collision policies for surface classes 0..7 in both observatory rooms.
extern WorldCollisionSurfaceProperties* gAkropolisObservatorySurfacePropertiesByClass[AKROPOLIS_OBSERVATORY_SURFACE_CLASS_COUNT];

/// Emits the observatory's lens flares at anchors enabled for the mapped view.
void akropolisObservatoryLensFlareEmitterTask(Task* task);

/// Draws one frame of the observatory's additive ambient glow, then retires its effect.
void akropolisObservatoryAmbientGlowTask(Task* task);

/// Runs the observatory's room messages and arrival scene gates.
void akropolisObservatoryRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_OBSERVATORY_H
