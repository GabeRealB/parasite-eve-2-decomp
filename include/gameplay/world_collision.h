#ifndef GAMEPLAY_WORLD_COLLISION_H
#define GAMEPLAY_WORLD_COLLISION_H

struct Task;

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "gameplay/geometry.h"

#include "main/session_types.h"

// Collision lists, contact records, room grids and collision updates.

void Gp_TickWorldCollision(struct Task* unused);

extern s32 Gp_RoomParams[8];

extern WorldCollisionGrid* Gp_GridParams;

extern GpObj3A* D_80115550;

s32 func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3);

s32 func_800DFCCC(GpObj3A* arg0, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3);

extern WorldCollisionTrigger* Gp_PendingObj4C;

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1);

/// Averages the first `arg2` `WorldCollisionContact` records of `arg0` into `arg1`
/// (a 16.16 delta scaled by 16) and, when `arg3` is non-NULL, stores the
/// `1 << key` bitmask of the contributing records there. Records
/// below the floor cutoff are averaged separately and added on top.
/// Returns 0 when nothing contributed, 2 when two records push in
/// opposing directions, and 1 otherwise.
s32 func_800E0C10(WorldCollisionContact* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);

/// Accumulates the push-back of the first `arg2` `WorldCollisionContact` records of
/// `arg0` into `arg1` (a 16.16 delta scaled by 16) and, when `arg3` is
/// non-NULL, stores the `1 << key` bitmask of the contributing
/// records there. Returns 0 when nothing contributed, 2 when two kind-0
/// records push in opposing directions, and 1 otherwise.
s32 func_800E0FEC(WorldCollisionContact* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);

void Gp_LinkObj(s32 arg0, WorldCollisionBody* arg1);

void Gp_UnlinkObj(WorldCollisionBody* node);

void Gp_LinkObj4A(s32 arg0, WorldCollisionTrigger* arg1);

void Gp_UnlinkObj4A(s32 arg0, WorldCollisionTrigger* arg1);

/// Clears and initializes the complete caller-owned contact table.
///
/// `count` is the positive number of elements; its final element receives LAST.
/// The third argument is unused and retained for the resident interface.
void Gp_InitRec18Table(WorldCollisionContact* contacts, s32 count, s32 unused);

/// Last occupied contact matching `key`, as a 1-based index, or 0.
///
/// A zero search key returns 1 if any entry is occupied. The initialized table
/// must retain its final-element marker; holes are allowed.
s32 Gp_FindRec18(WorldCollisionContact* contacts, s32 key);

/// Number of occupied contacts whose packed high halfword equals `kind`.
///
/// Supply `kind` in its word position (for example, class 2 is 0x20000).
/// Traversal ends at the initialized table's final-element marker.
s32 Gp_CountRec18Hi(WorldCollisionContact* contacts, s32 kind);

/// Resets occupied entries while retaining the final-element marker.
///
/// Key, distance and vector components are cleared; the SDK vector pad
/// halfwords are retained. Empty entries are left as they were.
void Gp_ClearRec18Occupied(WorldCollisionContact* contacts);

s32 func_800E1ACC(u8* arg0);

s32 func_800E1B24(s32 arg0);

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2);

/// Builds a rotation matrix in `arg1` that orients along normalized `arg0`
/// (yaw from XZ, pitch from Y vs the XZ length, then roll by `arg2`).
void Gp_OrientAlong(VECTOR* arg0, MATRIX* arg1, s32 arg2);

#endif // GAMEPLAY_WORLD_COLLISION_H
