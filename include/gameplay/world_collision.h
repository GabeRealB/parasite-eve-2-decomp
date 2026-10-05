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

extern WorldCollisionOccluder* D_80115550;

s32 func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3);

s32 func_800DFCCC(WorldCollisionOccluder* occluder, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3);

extern WorldCollisionTrigger* Gp_PendingObj4C;

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1);

/// Averages the first `arg2` `WorldCollisionContact` records of `arg0` into `delta`
/// (a signed 16.16 world-space correction) and, when `arg3` is non-NULL, stores the
/// `1 << key` bitmask of the contributing records there. Records
/// below the floor cutoff are averaged separately and added on top.
/// Returns 0 when nothing contributed, 2 when two records push in
/// opposing directions, and 1 otherwise.
s32 func_800E0C10(WorldCollisionContact* arg0, WorldCollisionDelta* delta, s32 arg2, s32* arg3);

/// Accumulates the push-back of the first `arg2` `WorldCollisionContact` records of
/// `arg0` into `delta` (a signed 16.16 world-space correction) and, when `arg3` is
/// non-NULL, stores the `1 << key` bitmask of the contributing
/// records there. Returns 0 when nothing contributed, 2 when two kind-0
/// records push in opposing directions, and 1 otherwise.
s32 func_800E0FEC(WorldCollisionContact* arg0, WorldCollisionDelta* delta, s32 arg2, s32* arg3);

void Gp_LinkObj(s32 arg0, WorldCollisionBody* arg1);

/// Removes a borrowed body from its collision list, retaining only its shape kind.
///
/// A linked body must have the valid links installed by `Gp_LinkObj`; its
/// predecessor link and any successor's back-link are repaired before its own
/// links are cleared. Pass enables and body-index flags are cleared. Unlinked
/// bodies are left unchanged. Neither the body nor its context/contact storage
/// is freed or reset; owners must restore flags before linking it again.
void worldCollisionUnlinkBody(WorldCollisionBody* body);

void Gp_LinkObj4A(s32 arg0, WorldCollisionTrigger* arg1);

void Gp_UnlinkObj4A(s32 arg0, WorldCollisionTrigger* arg1);

/// Clears a caller-owned collision-contact table and marks its final entry.
///
/// `contacts` supplies writable storage for the positive `count` of elements.
/// All bytes, including vector pad halfwords, are zeroed before the last
/// entry receives `WORLD_COLLISION_CONTACT_LAST`. The owner keeps the table
/// alive while bodies borrow it; this function retains no pointer. `unused`
/// is ignored and retained for the exported interface; callers pass 0.
void worldCollisionInitContacts(WorldCollisionContact* contacts, s32 count, s32 unused);

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

/// Returns the highest surface-class bit present in one mask byte, or 0 for zero.
///
/// The result is 0..7; class 0 and an empty mask both return 0. When several
/// classes are present the highest wins. Reads exactly `*mask`, without
/// modifying or retaining it. A word mask may supply its first byte on this
/// little-endian target; higher bytes are ignored. Uses the SDK fixed-point
/// logarithm and changes its GTE leading-sign-bit-count state for a nonzero byte.
s32 worldCollisionSurfaceClassFromMask(const u8* mask);

/// Returns a grid contact key's surface class through a one-byte class mask.
///
/// The target shift forms `1U << (key & 31)` and classifies its low byte.
/// Valid room-grid keys
/// carry classes 0..7 in those low bits; packed category and response bits are
/// ignored. Low shift counts 8..31 produce an empty byte and return 0. Changes
/// the SDK logarithm's GTE leading-sign-bit-count state for a nonzero byte.
s32 worldCollisionSurfaceClassFromKey(s32 key);

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2);

#endif // GAMEPLAY_WORLD_COLLISION_H
