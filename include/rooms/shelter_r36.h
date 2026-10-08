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

// shelter_r36
extern u8* D_shelter_r36_8017E9BC[];

extern ViewCount D_shelter_r36_8017E9C0[];

extern DirectionWarpEntry D_shelter_r36_8017E9C4[];

extern WorldCollisionGrid D_shelter_r36_8017EAB0;

extern ViewCamera D_shelter_r36_8017EAD4[];

extern SpriteView D_shelter_r36_8017F318[];

extern WorldCoordRoomLights D_shelter_r36_8017F6DC;

extern WorldCollisionTrigger D_shelter_r36_8017F6F4[];

extern WorldCollisionSurfaceProperties* D_shelter_r36_8017FAE4[];

/// Runs the R36 room controller's initialization, message service or teardown.
///
/// `task->state` must be 0..2: arrival setup, idle service and release.
/// The map overlay spawns this zero-body task for stage 4, area 36. Keep the
/// room overlay and its resources loaded while the controller is live.
void shelterR36RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R36_H
