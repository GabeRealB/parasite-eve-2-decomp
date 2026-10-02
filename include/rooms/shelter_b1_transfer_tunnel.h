#ifndef INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_transfer_tunnel_801830B8[22];

// shelter_b1_transfer_tunnel
extern u8* D_shelter_b1_transfer_tunnel_80182954[];

extern ViewCount D_shelter_b1_transfer_tunnel_80182958[];

extern DirectionWarpEntry D_shelter_b1_transfer_tunnel_8018295C[];

extern WorldCollisionGrid D_shelter_b1_transfer_tunnel_80182AEC;

extern ViewCamera D_shelter_b1_transfer_tunnel_80182B10[];

extern SpriteView D_shelter_b1_transfer_tunnel_80182BE0[];

extern WorldCoordRoomLights D_shelter_b1_transfer_tunnel_80182D90;

extern WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182DA8[];

extern WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182ED8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_transfer_tunnel_80183184[];

void func_shelter_b1_transfer_tunnel_8017D678(Task* task);

void func_shelter_b1_transfer_tunnel_8018092C(Task* arg0);

void func_shelter_b1_transfer_tunnel_80181390(Task* task);

void func_shelter_b1_transfer_tunnel_80181C78(Task* task);

void func_shelter_b1_transfer_tunnel_8017E308(Task* task);

void func_shelter_b1_transfer_tunnel_8017F050(Task* arg0);

void func_shelter_b1_transfer_tunnel_8017F3E8(Task* arg0);

void func_shelter_b1_transfer_tunnel_801807F8(Task* arg0);

void func_shelter_b1_transfer_tunnel_8017D6D0(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
