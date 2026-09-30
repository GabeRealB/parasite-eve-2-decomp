#ifndef GAMEPLAY_PRIVATE_WORLD_COLLISION_H
#define GAMEPLAY_PRIVATE_WORLD_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"
#include "gameplay/enemy.h"

// Collision lists, contact records, room grids and collision updates.

/// Pair-handler table used by `Gp_RunPairHandler` / `Gp_CollideLists`.
/// Indexed by `GpPairRule.handler` (`Gp_PairNop` / `Gp_PairHandler1` /
/// `Gp_PairHandler3`).
extern GpPairFn Gp_PairHandlers[5];

/// Pair-rule table used by `Gp_RunPairHandler` / `Gp_CollideLists`, one rule
/// per ordered pair of body kinds. Rows and columns are `(flags & 7) - 1`.
extern GpPairRule D_8010FA4C[4][4];

/// Set to 1 by `Gp_TakePendingObj4C` when a pending `Gp_PendingObj4C` node is found;
/// `Gp_TickWorldCollision` then calls `Gp_ClearPendingObj4C` to clear those flags.
extern s32 Gp_PendingObj4CFlag;

void Gp_CollideObjGrid(WorldCollisionBody* arg0);

void Gp_CollideObjGridDir(WorldCollisionBody* arg0);

s32 func_800DD324(s32 faceId, VECTOR* seg, SVECTOR* ray, WorldCollisionBody* arg3);

extern GpObj4C* Gp_Obj4CList;

void func_800DD940(WorldCollisionBody* arg0);

void func_800DDDF8(WorldCollisionBody* obj);

void func_800DEC80(WorldCollisionBody* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3);

void func_800DEF80(WorldCollisionBody* node, GpObj4C* other);

void func_800DF6AC(WorldCollisionBody* node, GpObj4C* other, VECTOR3* from);

struct Enemy;

/// The nine list heads `Gp_ObjLists` points at. Each is a bare `WorldCollisionBody*`
/// whose address is the first link. A node's `prev` points to the link that
/// contains it, either this head or the preceding node's `next`.
/// `Gp_TickWorldCollision` runs `Gp_CollideListGrid` over each list and
/// `Gp_CollideLists` over the pairs that can interact.
extern WorldCollisionBody* Gp_ObjList0;

extern WorldCollisionBody* Gp_ObjList1;

extern WorldCollisionBody* Gp_ObjList2;

extern WorldCollisionBody* Gp_ObjList3;

extern WorldCollisionBody* Gp_ObjList4;

extern WorldCollisionBody* Gp_ObjList6;

extern WorldCollisionBody* Gp_ObjList7;

extern WorldCollisionBody* Gp_ObjList8;

void Gp_ClearObjHeads(void);

s32 Gp_PairNop(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind);

void Gp_ClearObj4AList(s32 arg0);

void Gp_LinkObj3A(s32 arg0, GpObj3A* arg1);

void Gp_ClearObj3AList(s32 arg0);

void Gp_LoadRoomParams(void);

void Gp_CommitObj4CSave(void);

void Gp_ClaimSlot18(struct Enemy* arg0, s32 arg1);

void Gp_CollideLists(WorldCollisionBody* a, WorldCollisionBody* b);

void Gp_CollideListGrid(WorldCollisionBody* node);

void func_800E0608(WorldCollisionBody* node, s32 mask, s32 match);

void func_800E06AC(WorldCollisionBody* node, s32 mask, s32 match);

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1);

void Gp_ObjWorldPos(WorldCollisionBody* arg0, VECTOR3* arg1);

void func_800E0994(WorldCollisionBody* arg0, VECTOR* arg1, SVECTOR* arg2);

void Gp_ClearPendingObj4C(void);

#endif // GAMEPLAY_PRIVATE_WORLD_COLLISION_H
