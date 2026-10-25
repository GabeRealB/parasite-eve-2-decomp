#ifndef INCLUDE_ROOMS_SHELTER_R49_H
#define INCLUDE_ROOMS_SHELTER_R49_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Slots in the room's movie-to-observatory transition task table.
enum {
    SHELTER_R49_OBSERVATORY_TRANSITION_TASK_INDEX = 0,
    SHELTER_R49_OBSERVATORY_MOVIE_TASK_INDEX      = 1,
    SHELTER_R49_OBSERVATORY_TASK_COUNT            = 2,
};

/// Extents of Shelter R49's room, mapped-view and placement-variant tables.
enum {
    SHELTER_R49_ROOM_COUNT         = 1,
    SHELTER_R49_VIEW_COUNT         = 3,
    SHELTER_R49_AREA_VARIANT_COUNT = 13,
};

/// Bodyless controllers for playing the room movie and entering Neo Ark's observatory.
///
/// Slot 0 starts the movie and then reloads the destination; slot 1 owns movie
/// presentation and restoration. Both start in state 0 at priority `TASK_PRIORITY_CONTROL` and use
/// neither descriptor data nor spawn payloads. Spawning copies the descriptor;
/// keep this overlay and the movie resources loaded until the tasks finish.
extern TaskDesc gShelterR49ObservatoryTransitionTasks[SHELTER_R49_OBSERVATORY_TASK_COUNT];

/// Placement layouts for the room's scripted scene actor.
///
/// Indexed directly by placement variant 0..12; only `AREA_DEFAULT_VARIANT` (1)
/// has placements and resources. The stage directory shares Mine Refuge's saved
/// placement state. Keep this overlay and actor_143900 loaded while the layout
/// records or spawned actor are in use.
extern AreaVariant gShelterR49AreaVariants[SHELTER_R49_AREA_VARIANT_COUNT];

/// Collision resources for the single room: a floor grid without triggers or occluders.
///
/// Indexed by room minus one, with room 1 the only valid selector. Setup borrows
/// the mutable grid and binds its view coordinate; keep the overlay loaded while active.
extern WorldCollisionRoomResources gShelterR49RoomCollisionResources[SHELTER_R49_ROOM_COUNT];

/// Logical-view maps for the single room, borrowing gameplay's identity map.
///
/// Room 1 maps logical views 1..3 to the same camera, image and sprite indices.
/// The shared map's capacity is independent of this room's three-view limit.
/// Both gameplay and this directory's overlay must remain loaded during lookup.
extern u8* gShelterR49RoomViewMaps[SHELTER_R49_ROOM_COUNT];

/// Reverse-lookup limits for the single room's logical-view map.
///
/// Room 1 has three logical views; this signed-halfword count is in elements.
/// The stage map borrows the array while this overlay is loaded.
extern ViewCount gShelterR49RoomViewCounts[SHELTER_R49_ROOM_COUNT];

/// Lighting for the single room, with one point light and default ambient minima.
///
/// Indexed by room minus one. Light transforms and attenuation stay writable;
/// keep the overlay loaded while the collection is borrowed.
extern WorldCoordRoomLighting gShelterR49RoomLighting[SHELTER_R49_ROOM_COUNT];

/// The room's sole arrival/departure endpoint, selected by warp 1.
///
/// Places the player at (0, -10000, 0) and companion at the origin, both facing
/// yaw 1024 (4096 units per turn), and selects view 2. Sounds, map updates and
/// special transition flags are disabled. Borrowed only while this overlay is loaded.
extern DirectionWarpEntry gShelterR49WarpEntries[1];

/// Three authored cameras, indexed by mapped view minus one.
///
/// Rotation coefficients use `ONE` for unity; translation is the negated camera
/// origin in world units, and projection distances are pixels. A queued camera
/// application requires this overlay to remain loaded until it reads the record.
extern ViewCamera gShelterR49ViewCameras[SHELTER_R49_VIEW_COUNT];

/// Sprite resources for mapped views 1..3, retaining decoded-image backgrounds.
///
/// View 2 adds fifteen depth-sorted sprites; views 1 and 3 have no source sprites.
/// No view supplies clipping areas. The descriptor and mutable source/batch arrays
/// belong to this overlay and must stay loaded while packets are built or linked.
extern SpriteView gShelterR49SpriteViews[SHELTER_R49_VIEW_COUNT];

/// Collision policies for surface classes 0..7, with footstep cues on floor class 1.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Other classes share the record without footstep cues. The pointer directory
/// and its records remain borrowed only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterR49SurfacePropertiesByClass[8];

/// Runs Shelter R49's room initialization, idle or teardown state.
///
/// The stage's room-task table starts this bodyless task in state 0. Initialization
/// installs its message handlers, publishes it in the room slot and starts the
/// room scene unless the live save's demo scene is 9. State 1 remains idle;
/// state 2 kills the task. The state must stay in 0..2; dispatch has no bounds
/// check. Requires the room, map and scene-actor overlays to remain loaded while
/// their callbacks or scripts can run. No work block or spawn payload is used.
void shelterR49RoomTask(Task* task);

/// No-op per-frame callback for Shelter R49's room-effect task.
///
/// Effect bank 6, slot 0x162 supplies a coordinate-body task. `unusedTask` is
/// ignored; the callback draws nothing and leaves the task and its resources
/// live for external teardown. The `shelter_r49` overlay must remain loaded
/// while this callback can run.
void shelterR49EffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_R49_H
