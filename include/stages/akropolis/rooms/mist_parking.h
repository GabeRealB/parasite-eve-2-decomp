#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_PARKING_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_PARKING_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Bodyless priority-192 display task for this room's shop UI session.
extern TaskDesc gMistParkingShopSessionTaskDesc;

/// Number of retained placement-variant slots, including absent slots.
enum { MIST_PARKING_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
extern AreaVariant gMistParkingAreaVariants[MIST_PARKING_AREA_VARIANT_COUNT];

/// Dimensions of the parking area's room variants, logical views and surface classes.
enum {
    MIST_PARKING_ROOM_VARIANT_COUNT  = 4,
    MIST_PARKING_VIEW_COUNT          = 20,
    MIST_PARKING_WARP_COUNT          = 4,
    MIST_PARKING_SURFACE_CLASS_COUNT = 8
};

/// Collision resources for parking room variants 1..4, indexed by room - 1.
extern WorldCollisionRoomResources gMistParkingRoomCollisionResources[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Logical-view maps for the parking area's four room variants.
extern u8* gMistParkingViewMaps[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Logical-view search lengths for parking room variants 1..4, indexed by room - 1.
extern ViewCount gMistParkingViewCounts[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Light collections and ambient minima for the parking area's four room variants.
extern WorldCoordRoomLighting gMistParkingRoomLightingTable[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Four parking arrival/departure endpoints, indexed by 1-based warp ID minus one.
extern DirectionWarpEntry gMistParkingWarpEntries[MIST_PARKING_WARP_COUNT];

/// Cameras for mapped parking view slots 1..20, indexed by mapped index minus one.
extern ViewCamera gMistParkingViewCameras[MIST_PARKING_VIEW_COUNT];

/// Sprite resources for mapped parking view slots 1..20, indexed by mapped index minus one.
extern SpriteView gMistParkingSpriteViews[MIST_PARKING_VIEW_COUNT];

/// Collision policies and footstep cues indexed by parking surface class 0..7.
extern WorldCollisionSurfaceProperties* gMistParkingSurfacePropertiesByClass[MIST_PARKING_SURFACE_CLASS_COUNT];

/// Placement choices for the collision patch controlled with Pierce's visibility.
enum {
    MIST_PARKING_PIERCE_PATCH_RESTORED = 0,
    MIST_PARKING_PIERCE_PATCH_LOWERED  = 1
};

/// Restores Pierce's collision patch, optionally lowering it by 2000 world Y units.
void mistParkingSetPierceCollisionPatchLowered(s32 lowerPatch);

/// Updates the parking-lot telephone's save menu and optional play statistics.
void mistParkingTelephoneMenuTask(Task* task);

/// Starts the selected parking departure script once EVS and CAP are idle.
void mistParkingContinueDepartureChoiceTask(Task* task);

/// Draws the current mapped camera view's grey capsule glows and pulsing cyan point glow.
void mistParkingDrawGlowsTask(Task* unused);

/// Runs the parking-lot room's message receiver and arrival conversation setup.
void mistParkingRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_PARKING_H
