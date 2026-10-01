#ifndef INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
#define INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_secret_passage_80183340[12];

// mine_secret_passage
extern GpRoomCoordRec D_mine_secret_passage_80180F9C[];

extern GpRoomObjRec D_mine_secret_passage_80180FA4[];

extern u8* D_mine_secret_passage_80180FB4[];

extern GpViewCountRec D_mine_secret_passage_80180FB8[];

extern GpWarpRec D_mine_secret_passage_80180FBC[];

extern GpViewRec D_mine_secret_passage_80181604[];

extern GpSprtRec D_mine_secret_passage_80182994[];

extern WorldCollisionSurfaceProperties* D_mine_secret_passage_80183420[];

void func_mine_secret_passage_8017F948(Task* arg0);

void func_mine_secret_passage_8017F5B0(Task* arg0);

void func_mine_secret_passage_8017E868(Task* task);

void func_mine_secret_passage_80180D58(Task* arg0);

void func_mine_secret_passage_8017D9D4(Task* arg0);

void func_mine_secret_passage_8017D970(Task* task);

#endif // INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
