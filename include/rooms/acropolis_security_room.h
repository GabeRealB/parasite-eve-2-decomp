#ifndef INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H
#define INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_acropolis_security_room_80184088[4];

extern TmdSource gAcropolisSecurityRoomAcropolisSanctuaryModel090F0;

// acropolis_security_room
extern WorldCollisionRoomResources D_acropolis_security_room_801839D0[];

extern u8* D_acropolis_security_room_801839E0[];

extern ViewCount D_acropolis_security_room_801839E4[];

extern WorldCoordRoomLighting D_acropolis_security_room_801839E8[];

extern DirectionWarpEntry D_acropolis_security_room_801839F0[];

extern SpriteView D_acropolis_security_room_80184C50[];

extern ViewCamera D_acropolis_security_room_80184D10[];

extern WorldCollisionSurfaceProperties* D_acropolis_security_room_80184FA0[];

void func_acropolis_security_room_801805A4(Task* task);

void func_acropolis_security_room_80180E34(Task* arg0);

void func_acropolis_security_room_80181108(Task* arg0);

void func_acropolis_security_room_801817A4(Task* task);

void func_acropolis_security_room_8017ED68(Task* task);

void func_acropolis_security_room_80180294(Task* task);

void func_acropolis_security_room_8017D984(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H
