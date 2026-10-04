#ifndef INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H
#define INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern u8 D_acropolis_bridge_80189A9C[24];

extern AreaVariant D_acropolis_bridge_8019004C[12];

// acropolis_bridge
extern WorldCollisionRoomResources D_acropolis_bridge_80189A54[];

extern u8* D_acropolis_bridge_80189A80[];

extern ViewCount D_acropolis_bridge_80189A88[];

extern WorldCoordRoomLighting D_acropolis_bridge_80189A8C[];

extern DirectionWarpEntry D_acropolis_bridge_80189AB4[];

extern SpriteView D_acropolis_bridge_8018FFA4[];

extern ViewCamera D_acropolis_bridge_80190A24[];

extern WorldCollisionSurfaceProperties* D_acropolis_bridge_80190C34[];

void func_acropolis_bridge_8017F868(Task* task);

void func_acropolis_bridge_801812F4(Task* task);

void func_acropolis_bridge_801819C8(Task* task);

void func_acropolis_bridge_80181D28(Task* task);

void func_acropolis_bridge_80180320(Task* task);

void func_acropolis_bridge_8018063C(Task* task);

void func_acropolis_bridge_8018099C(Task* task);

void func_acropolis_bridge_80180CC0(Task* task);

void func_acropolis_bridge_80180FF0(Task* task);

void func_acropolis_bridge_80182694(Task* task);

void acropolisBridgeEffectSpriteDebrisTask(Task* task);

void func_acropolis_bridge_80182394(Task* task);

void func_acropolis_bridge_8017F788(Task* task);

void func_acropolis_bridge_8017DA0C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_BRIDGE_H
