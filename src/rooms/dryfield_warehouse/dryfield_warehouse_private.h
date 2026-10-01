#ifndef SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

/// The descriptors of the warehouse's cutscene tasks, spawned by index: the
/// cutscene task itself, the screen fade that ramps its channels up from 0, and
/// the one that ramps them down from 0xFF.
extern TaskDesc D_dryfield_warehouse_8017FB08[];

extern WorldCollisionTrigger D_dryfield_warehouse_801816A4[4];

extern WorldCollisionTrigger D_dryfield_warehouse_801817D4[13];

extern WorldCollisionTrigger D_dryfield_warehouse_80181BB0[10];

extern WorldCoordRoomLights D_dryfield_warehouse_801820E8[1];

extern Task* D_dryfield_warehouse_801821BC;

extern Task* D_dryfield_warehouse_801821C0;

extern s16 D_dryfield_warehouse_801821C4;

extern GpMsgEntry D_dryfield_warehouse_8017F554[3];

extern TaskDesc D_dryfield_warehouse_8017F56C[2];

extern SpriteBatch D_dryfield_warehouse_801811A0[2];

extern SpriteSource D_dryfield_warehouse_801811B0[21];

extern SpriteBatch D_dryfield_warehouse_80181354[6];

extern SpriteSource D_dryfield_warehouse_80181384[25];

extern SpriteBatch D_dryfield_warehouse_80181578[6];

extern SpriteSource D_dryfield_warehouse_801815A8[2];

extern SpriteBatch D_dryfield_warehouse_801815D0[3];

extern SpriteBatch D_dryfield_warehouse_801815E8[2];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_warehouse_8017D5E8(Task*);

s32 func_dryfield_warehouse_8017D764(Task*, s32, s32, TaskMessageArg);

s32 func_dryfield_warehouse_8017D824(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_dryfield_warehouse_8017D8D4(Task*);

#endif // SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
