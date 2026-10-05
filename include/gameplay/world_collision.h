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

/// Probes from `start` toward `target` against blocking faces of the active room grid.
///
/// Inputs and optional `hitPoint` use view-space game coordinates. The optional
/// `surfaceNormal` receives the grid's unrotated room-space normal, 4096 per unit.
/// Returns 1 on a hit, else 0, including when no grid is active. Successive hits
/// clip the target end while retaining the original direction and fixed start.
/// Outputs may alias inputs; only XYZ are written, pad halfwords are untouched,
/// and a no-hit result leaves both outputs unchanged.
///
/// A live grid must fit the 256-face candidate mask and have valid cell lists,
/// mesh indices, unit normals and a composed view transform. Supply a nonzero
/// segment whose delta fits signed halfwords and squared length in 1..0x7FFFFFFF;
/// mesh edges must meet the same SDK normalization bounds. Query storage must
/// stay clear of the initialized scratch stack's 176-byte peak reservation.
/// Clears the shared candidate mask, changes GTE state and retains no pointers.
s32 worldCollisionProbeGridSegment(const SVECTOR* target, const SVECTOR* start, SVECTOR* hitPoint, SVECTOR* surfaceNormal);

/// Returns 1 when a segment crosses an occluder quad, else 0.
///
/// `start` and `end` are view-space positions in game units; `direction` is
/// end minus start normalized with 4096 per unit. The current composed view
/// matrix places the room-space quad. Both crossing directions are accepted;
/// endpoint intersections and parallel segments are rejected, while intersections
/// on quad edges are accepted. The occluder's enable flag and radius are not tested.
/// Plane and edge offsets narrow to signed halfwords; retained signed word
/// arithmetic and division determine the intersection. Inputs are unchanged.
///
/// All inputs and transforms must be live and clear of the initialized scratch
/// stack's 128-byte reservation. Releases the block on every exit, changes GTE
/// state and retains no pointers.
s32 worldCollisionTestOccluderSegment(const WorldCollisionOccluder* occluder, const SVECTOR* start, const SVECTOR* end, const VECTOR* direction);

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

/// Collision groups selected when linking a body; group membership determines pair-test partners.
///
/// Enemy-body lists also carry shootable scenery and movement probes; enemy
/// attack lists also carry sight probes. Player bodies include companions.
/// Slots 5 and 6 have no current link callers: 5 has no passes and 6 is tested
/// only against player attacks. GRID_ONLY receives no pair tests. BLASTS are
/// tested against player bodies, enemy bodies and props.
enum {
    WORLD_COLLISION_LIST_PLAYER_BODIES  = 0,
    WORLD_COLLISION_LIST_PLAYER_ATTACKS = 1,
    WORLD_COLLISION_LIST_ENEMY_BODIES   = 2,
    WORLD_COLLISION_LIST_ENEMY_ATTACKS  = 3,
    WORLD_COLLISION_LIST_PROPS          = 4,
    WORLD_COLLISION_LIST_GRID_ONLY      = 7,
    WORLD_COLLISION_LIST_BLASTS         = 8
};

/// Appends a borrowed body to a collision group, preserving insertion order.
///
/// `listIndex` is a group index in 0..8, not a priority. `body` must be non-NULL;
/// already-linked bodies and shape kinds 5..7 are left unchanged. Kinds 0..4
/// are accepted, including the no-shape kind. Linking sets LINKED and installs
/// the forward and predecessor links without changing any other flags or
/// initializing the shape or contacts. Owners keep the body, its transform,
/// context and initialized contact storage alive until unlinking.
void worldCollisionLinkBody(s32 listIndex, WorldCollisionBody* body);

/// Removes a borrowed body from its collision list, retaining only its shape kind.
///
/// A linked body must have the valid links installed by `worldCollisionLinkBody`; its
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

/// Search key selecting any occupied contact instead of an exact packed identity.
enum { WORLD_COLLISION_FIND_ANY_KEY = 0 };

/// Returns the last occupied contact matching `searchKey`, as a one-based index, or 0.
///
/// `WORLD_COLLISION_FIND_ANY_KEY` returns 1 as soon as any entry is occupied,
/// regardless of its index or key. Other keys match the entire packed word.
/// Supply a non-NULL readable table ending in WORLD_COLLISION_CONTACT_LAST;
/// that final entry is included and holes are allowed. The table is unchanged,
/// and no pointer is retained.
s32 worldCollisionFindContactIndex(const WorldCollisionContact* contacts, s32 searchKey);

/// Counts occupied contacts of one packed-key category.
///
/// `contactKind` is already shifted into the high halfword, as in
/// `WORLD_COLLISION_CONTACT_PLAYER_BODY`, `WORLD_COLLISION_CONTACT_ENEMY_BODY`
/// or `WORLD_COLLISION_CONTACT_GRID`; the low halfword must be zero.
/// Supply a non-NULL readable table ending in `WORLD_COLLISION_CONTACT_LAST`.
/// The final entry is included, empty entries are skipped and identities in
/// the low halfword are ignored. Returns an element count, including zero;
/// the table is unchanged and no pointer is retained.
s32 worldCollisionCountContactsByKind(const WorldCollisionContact* contacts, s32 contactKind);

/// Empties occupied contacts while preserving the table's final-entry marker.
///
/// Supply a non-NULL writable table ending in WORLD_COLLISION_CONTACT_LAST;
/// the final entry is included. Occupied entries lose every other flag and
/// their key, distance, point and response components, including any encoded
/// body address. The two SDK vector pad halfwords and all empty entries are
/// untouched. The owner retains the storage; no pointer is retained or freed.
void worldCollisionClearContacts(WorldCollisionContact* contacts);

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
