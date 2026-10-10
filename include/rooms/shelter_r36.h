#ifndef INCLUDE_ROOMS_SHELTER_R36_H
#define INCLUDE_ROOMS_SHELTER_R36_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_r36_8017FA40[11];

/// R36's single room entry, mapping logical views to camera/image/sprite indices.
///
/// Indexed by room - 1, with room 1 borrowing `gViewIdentityMap` unchanged.
/// R36 has 11 logical views; that count is independent of the shared map's
/// capacity. The directory stays valid while this room overlay is loaded.
extern u8* gShelterR36RoomViewMaps[1];

extern ViewCount D_shelter_r36_8017E9C0[];

extern DirectionWarpEntry D_shelter_r36_8017E9C4[];

/// R36's class-1 floor quad and its 3 by 3 collision-cell index.
///
/// The floor lies at Y = 0, spanning X and Z from -4000 to 4000 game units.
/// Cells are 4000 units wide with origin (-4000, -4000); all nine lists contain
/// the floor face. Activation binds the mutable grid to the current view.
/// The descriptor and its private mesh/index arrays belong to this overlay
/// and must remain loaded throughout collision queries.
extern WorldCollisionGrid gShelterR36FloorCollisionGrid;

extern ViewCamera D_shelter_r36_8017EAD4[];

extern SpriteView D_shelter_r36_8017F318[];

/// R36's four directional and five point lights for model shading.
///
/// All lights accept every view; there are no cone lights. Coordinate updates
/// and lighting queries mutate the private light arrays, which belong to this
/// overlay and remain valid only while it is loaded.
extern WorldCoordRoomLights gShelterR36RoomLights;

extern WorldCollisionTrigger D_shelter_r36_8017F6F4[];

/// R36's collision and footstep policies, indexed by surface class 0..7.
///
/// Classes 0, 4, 6 and 7 share a blocking policy without footsteps. Class 1
/// is the floor: probes pass and weapon impacts are ignored. Classes 2, 3 and 5
/// block probes and carry distinct footstep request tables. Every class enables
/// pushback. Records and sound tables are borrowed from this loaded overlay;
/// the floor mesh uses only class 1.
extern WorldCollisionSurfaceProperties* gShelterR36SurfacePropertiesByClass[8];

/// Runs the R36 room controller's initialization, message service or teardown.
///
/// `task->state` must be 0..2: arrival setup, idle service and release.
/// The map overlay spawns this zero-body task for stage 4, area 36. Keep the
/// room overlay and its resources loaded while the controller is live.
void shelterR36RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R36_H
