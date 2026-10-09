#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_garage_80183380[2];

extern TaskDesc D_dryfield_night_garage_80181C2C;

extern AreaVariant D_dryfield_night_garage_801874BC[12];

/// Lighting for night-garage rooms 1 and 2, indexed by room ID minus one.
///
/// Both entries borrow the same seven point lights and logical-view ambient
/// minima. The records and their resources remain valid while this room overlay
/// is loaded; light transforms and attenuation are writable query state.
extern WorldCoordRoomLighting gDryfieldNightGarageRoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_garage_80183404[];

/// Logical-view maps for night-garage rooms 1 and 2, indexed by room ID minus one.
///
/// Each map accepts views 1..15 at index view minus one and returns a 1-based
/// camera, background-file and sprite-view index. Room 1 borrows gameplay's
/// identity map; room 2 remaps several views to alternate images. The directory
/// and room-2 map require the room overlay to remain loaded.
extern u8* gDryfieldNightGarageViewMaps[2];

extern ViewCount D_dryfield_night_garage_8018343C[];

extern DirectionWarpEntry D_dryfield_night_garage_80183440[];

extern ViewCamera D_dryfield_night_garage_801843F8[];

extern SpriteView D_dryfield_night_garage_80186258[];

/// Collision policies for the night garage's eight surface classes, indexed 0..7.
///
/// Every class blocks probes and permits weapon impacts and pushback. Class 1
/// supplies footstep cues; classes 0 and 2..7 share a silent policy. Both room
/// grids use this directory. The borrowed records and sound data remain valid
/// only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightGarageSurfacePropertiesByClass[8];

/// Places the low collision box in the live night-garage mesh.
///
/// Replaces its first four faces, four normals and eight vertices with the box
/// template, then translates XYZ by (4715, -132, 5900) game units for zero
/// `useFarPosition`, or (4715, -132, 10000) for nonzero. Gary Douglas's setup
/// uses zero before the refueling event; both completion and skip scripts use
/// nonzero. Only XYZ and the face records are replaced; the vector fourth
/// components, remaining mesh and cell lists stay intact. Requires the loaded
/// room overlay and writable live grid pools, which the room owns.
void dryfieldNightGaragePlaceLowCollisionBox(s32 useFarPosition);

/// Draws the night garage's additive grey capsule glows for the current view.
///
/// Gameplay effect bank 6, slot 0x114. Views 3 and 15 draw four strips; views
/// 7 and 14 draw the first strip; view 11 draws two strips with half-turned
/// capsule caps, sharing one strip with views 3 and 15. Other views draw none.
/// The callback ignores `unusedTask` and the spawn arguments. Requires the
/// loaded room overlay, composed view matrix and the current frame's
/// initialized scratch stack, ordering table and GPU packet arena.
void dryfieldNightGarageDrawGlowsTask(Task* unusedTask);

/// Runs the night garage's persistent room-message task.
///
/// `task->state` must be 0 (initialize), 1 (idle for synchronous messages), or
/// 2 (kill). Initialization registers the task in `GAME_TASK_SLOT_ROOM` and
/// advances to idle. Spawned by the Dryfield map's stage-3/area-24 descriptor;
/// requires the room overlay and that variant's actor/map resources to be live.
void dryfieldNightGarageRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
