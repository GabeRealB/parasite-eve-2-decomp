#ifndef INCLUDE_ROOMS_MINE_MESA_H
#define INCLUDE_ROOMS_MINE_MESA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_mesa_801898F4[12];

// mine_mesa
extern WorldCollisionRoomResources D_mine_mesa_80186538[];

extern u8* D_mine_mesa_80186548[];

extern WorldCoordRoomLighting D_mine_mesa_8018654C[];

extern ViewCount D_mine_mesa_80186554[];

extern GpWarpRec D_mine_mesa_80186558[];

extern ViewCamera D_mine_mesa_80187030[];

extern SpriteView D_mine_mesa_80188744[];

extern WorldCollisionSurfaceProperties* D_mine_mesa_80189A60[];

void func_mine_mesa_801811C4(s32 height);

void func_mine_mesa_8017F230(Task* task);

void func_mine_mesa_8017FC94(Task* task);

void func_mine_mesa_8018057C(Task* task);

void func_mine_mesa_8017ED08(Task* arg0);

/// Task entries the Shelter map UI overlay's stage tables name, each room's
/// entry task started for its location and the enemy descriptors' tasks, and
/// the models those descriptors attach.
void func_mine_mesa_8017DD98(Task* task);

#endif // INCLUDE_ROOMS_MINE_MESA_H
