#ifndef SRC_ROOMS_DRYFIELD_FACTORY_DRYFIELD_FACTORY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_FACTORY_DRYFIELD_FACTORY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

extern WorldCollisionTrigger D_dryfield_factory_80189694[14];

extern WorldCollisionTrigger D_dryfield_factory_80189ABC[20];

extern WorldCoordRoomLights D_dryfield_factory_8018A28C[1];

extern WorldCoordRoomAmbientEntry D_dryfield_factory_8018A2A4[20];

extern TaskDesc gRoomEventTaskDesc;

extern TaskDesc D_dryfield_factory_801826BC[2];

extern TaskMessageEntry D_dryfield_factory_801826D4[6];

extern WorldCollisionGrid D_dryfield_factory_80186C68;

extern WorldCollisionGrid D_dryfield_factory_80186D38;

extern WorldCollisionGrid D_dryfield_factory_80186E04;

extern TaskDesc D_dryfield_factory_80186E94[1];

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_factory_8017DB08(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_dryfield_factory_8017DD00(Task*);

void func_dryfield_factory_8017FC18(Task*);

void func_dryfield_factory_8017FDDC(Task*);

void func_dryfield_factory_8018001C(Task*);

void func_dryfield_factory_801807DC(Task*);

void func_dryfield_factory_80180964(Task*);

#endif // SRC_ROOMS_DRYFIELD_FACTORY_DRYFIELD_FACTORY_PRIVATE_H
