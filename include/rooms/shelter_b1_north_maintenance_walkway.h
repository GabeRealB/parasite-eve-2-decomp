#ifndef INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_north_maintenance_walkway_80185A38[12];

// shelter_b1_north_maintenance_walkway
extern u8* D_shelter_b1_north_maintenance_walkway_80184B80[];

extern ViewCount D_shelter_b1_north_maintenance_walkway_80184B84[];

extern DirectionWarpEntry D_shelter_b1_north_maintenance_walkway_80184B88[];

extern WorldCollisionGrid D_shelter_b1_north_maintenance_walkway_80184F40;

extern ViewCamera D_shelter_b1_north_maintenance_walkway_80184F64[];

extern SpriteView D_shelter_b1_north_maintenance_walkway_801853AC[];

extern WorldCoordRoomLights D_shelter_b1_north_maintenance_walkway_801855D4;

extern WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_801855EC[];

extern WorldCollisionOccluder D_shelter_b1_north_maintenance_walkway_801857B4[];

extern WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_80185A98[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_north_maintenance_walkway_80185B4C[];

void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task);

void func_shelter_b1_north_maintenance_walkway_8017E8B8(Task* task);

void func_shelter_b1_north_maintenance_walkway_80180EDC(Task* arg0);

void func_shelter_b1_north_maintenance_walkway_80181940(Task* task);

void func_shelter_b1_north_maintenance_walkway_80182228(Task* task);

void func_shelter_b1_north_maintenance_walkway_80182E70(Task* arg0);

void func_shelter_b1_north_maintenance_walkway_801833C8(Task* task);

void func_shelter_b1_north_maintenance_walkway_80184028(Task* arg0);

void func_shelter_b1_north_maintenance_walkway_8017F600(Task* arg0);

void func_shelter_b1_north_maintenance_walkway_8017F998(Task* arg0);

void func_shelter_b1_north_maintenance_walkway_80180DA8(Task* arg0);

/// Binds the room's effect tasks once and draws the active view's lamp glows each frame.
///
/// Task state 0 selects the room effect IDs and advances to state 1; every state
/// draws glows in views 2..6, while other views emit none. Requires this room
/// overlay to remain loaded, composed view matrices, the glow drawers' scratch
/// stack capacity, and room in the current frame's packet arena and ordering table.
void shelterB1NorthMaintenanceWalkwayDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
