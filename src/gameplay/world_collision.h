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
/// Indexed by `WorldCollisionPairRule::handlerIndex` (`worldCollisionPairNop` / `Gp_PairHandler1` /
/// `Gp_PairHandler3`).
extern WorldCollisionPairHandler Gp_PairHandlers[5];

/// Pair-rule table used by `Gp_RunPairHandler` / `Gp_CollideLists`, one rule
/// per ordered pair of body kinds. Rows and columns are `(flags & 7) - 1`.
extern WorldCollisionPairRule D_8010FA4C[4][4];

/// Set to 1 by `Gp_TakePendingObj4C` when a pending `Gp_PendingObj4C` node is found;
/// `Gp_TickWorldCollision` then calls `Gp_ClearPendingObj4C` to clear those flags.
extern s32 Gp_PendingObj4CFlag;

void Gp_CollideObjGrid(WorldCollisionBody* arg0);

void Gp_CollideObjGridDir(WorldCollisionBody* arg0);

s32 func_800DD324(s32 faceId, VECTOR* seg, SVECTOR* ray, WorldCollisionBody* arg3);

extern WorldCollisionTrigger* Gp_Obj4CList;

void func_800DD940(WorldCollisionBody* arg0);

void func_800DDDF8(WorldCollisionBody* obj);

void func_800DEC80(WorldCollisionBody* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3);

void func_800DEF80(WorldCollisionBody* node, WorldCollisionTrigger* other);

void func_800DF6AC(WorldCollisionBody* node, WorldCollisionTrigger* other, VECTOR3* from);

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

/// Pair-dispatch callback for routes that perform no contact test.
///
/// Installed at handler indices 0, 2 and 4. Ignores both ordered bodies and
/// the handler index, changes no state, and returns 0 (no contact).
s32 worldCollisionPairNop(WorldCollisionBody* firstBody, WorldCollisionBody* secondBody, s32 handlerIndex);

void Gp_ClearObj4AList(s32 arg0);

void Gp_LinkObj3A(s32 arg0, WorldCollisionOccluder* occluder);

void Gp_ClearObj3AList(s32 arg0);

void Gp_LoadRoomParams(void);

void Gp_CommitObj4CSave(void);

void Gp_ClaimSlot18(struct Enemy* arg0, s32 arg1);

void Gp_CollideLists(WorldCollisionBody* a, WorldCollisionBody* b);

void Gp_CollideListGrid(WorldCollisionBody* node);

void func_800E0608(WorldCollisionBody* node, s32 mask, s32 match);

void func_800E06AC(WorldCollisionBody* node, s32 mask, s32 match);

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1);

/// Places a body's local centre/origin in its cached coordinate composition frame.
///
/// `body->coord->workm` must already be composed. Rotates `body->pos` and adds
/// the cached translation, in game-coordinate units; the frame may be the
/// view frame rather than absolute world axes. Writes XYZ only, leaving the
/// output's fourth word untouched. Body and output must be disjoint from an
/// initialized scratch stack's 48-byte reservation, released before return.
/// Changes GTE rotation and arithmetic state; retains no pointers and does not
/// compose or modify the body.
void worldCollisionGetBodyComposedPosition(const WorldCollisionBody* body, VECTOR* position);

/// Places a body's vertical floor-query segment and returns its unit direction.
///
/// In the body's frame, X and Z are zero and endpoint Y is `pos.vy + radius`
/// for [0], `pos.vy - radius` for [1], truncated to signed halfwords. The cached
/// `coord->workm` rotates and translates both into its composition frame, in
/// game-coordinate units; it must already be composed. `direction` is endpoint
/// 0 minus endpoint 1, normalized with 4096 for one unit. Endpoint pad words and
/// the direction pad halfword are untouched. Radius and transform must produce
/// a segment delta with signed-halfword components and squared length in
/// 1..0x7FFFFFFF, as required by the SDK's GTE normalization. Outputs must be
/// disjoint from each other, the
/// body and its transform, and the initialized scratch stack's 32-byte
/// reservation. Releases that reservation before return, changes GTE rotation
/// and arithmetic state, and retains no pointers.
void worldCollisionPlaceFloorSegment(const WorldCollisionBody* body, VECTOR endpoints[2], SVECTOR* direction);

void Gp_ClearPendingObj4C(void);

#endif // GAMEPLAY_PRIVATE_WORLD_COLLISION_H
