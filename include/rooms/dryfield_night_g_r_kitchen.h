#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_g_r_kitchen_8017EB88[12];

/// Extents of the nighttime kitchen's room, mapped-view and surface directories.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT          = 1,
    DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT          = 3,
    DRYFIELD_NIGHT_G_R_KITCHEN_SURFACE_CLASS_COUNT = 8,
};

/// One-based warp slots for the kitchen's two doors.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_WARP_SALOON      = 1,
    DRYFIELD_NIGHT_G_R_KITCHEN_WARP_WATER_TOWER = 2,
    DRYFIELD_NIGHT_G_R_KITCHEN_WARP_COUNT       = 2,
};

/// Lighting for room 1: four point lights and the gameplay default ambient minima.
///
/// Indexed by room minus one. The map borrows this array and its mutable lights
/// while the kitchen overlay remains loaded.
extern WorldCoordRoomLighting gDryfieldNightGRKitchenRoomLighting[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT];

/// Collision resources for room 1: the mesh, view boundaries and interaction quads.
///
/// Indexed by room minus one; there are no sight occluders. Gameplay binds the
/// borrowed grid and trigger arrays to the current view and mutates their links.
/// Keep the kitchen overlay loaded until the resources have been unlinked.
extern WorldCollisionRoomResources gDryfieldNightGRKitchenRoomCollisionResources[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT];

/// Room 1's logical-view map, borrowing gameplay's shared identity mapping.
///
/// Room and logical-view indices are one-based. Only logical views 1..3 select
/// this kitchen's camera, image and sprite entries; the shared map's capacity
/// does not extend those resources. Both overlays must remain loaded.
extern u8* gDryfieldNightGRKitchenViewMaps[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT];

/// Room 1's three logical views, limiting reverse searches of its view map.
///
/// Indexed by room minus one; counts are signed halfword element counts.
extern ViewCount gDryfieldNightGRKitchenViewCounts[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT];

/// Arrival placements and departure options for the saloon and water-tower doors.
///
/// Indexed by the one-based warp slot minus one. Both place the player and
/// companion in view 2, with yaw in 4096 units per turn and XYZ in room units.
/// Transitions copy entries by value while the kitchen overlay is loaded.
extern DirectionWarpEntry gDryfieldNightGRKitchenWarpEntries[DRYFIELD_NIGHT_G_R_KITCHEN_WARP_COUNT];

/// Camera transforms and pixel projection distances for mapped views 1..3.
///
/// Indexed by mapped view minus one; the identity map preserves logical order.
/// Records are borrowed until a queued camera application finishes.
extern ViewCamera gDryfieldNightGRKitchenViewCameras[DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT];

/// Sprite resources for mapped views 1..3, with seven source rectangles in view 2.
///
/// Indexed by mapped view minus one. Every view uses decoded background strips;
/// views 1 and 3 have no source sprites, and no view has a clipping-area list.
/// Descriptors and mutable batch/source arrays remain borrowed while loaded.
extern SpriteView gDryfieldNightGRKitchenSpriteViews[DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT];

/// Collision policies for surface classes 0..7, with footstep cues only on floor class 1.
///
/// All classes block probes, allow weapon impacts and apply pushback. The table
/// has no terminator; its records and cues are borrowed from the loaded kitchen.
extern WorldCollisionSurfaceProperties* gDryfieldNightGRKitchenSurfacePropertiesByClass[DRYFIELD_NIGHT_G_R_KITCHEN_SURFACE_CLASS_COUNT];

/// Draws the nighttime kitchen's two grey light shafts for view 2 or 3 each frame.
///
/// Other views emit no packets. The task argument is unused. Requires the room
/// overlay to remain loaded, a composed view, an initialized scratch stack and
/// space in the current frame's primitive arena and depth ordering table.
void dryfieldNightGRKitchenDrawLightShaftsTask(Task* unusedTask);

/// Runs the nighttime kitchen's room-message task.
///
/// Requires state 0..2: install the receiver, idle while messages handle
/// requests, then teardown. Keep the room overlay loaded throughout dispatch.
void dryfieldNightGRKitchenRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H
