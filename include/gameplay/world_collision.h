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

/// Returns 1 when any enabled sight occluder crosses the segment, else 0.
///
/// Endpoints are view-space positions in game units, in the current composed
/// view frame used to place the room's occluders. Endpoint and parallel-plane
/// intersections are rejected; quad edges and either crossing direction count.
/// The segment delta must fit signed halfwords and have squared length in
/// 1..0x7FFFFFFF for SDK normalization. Inputs are unchanged and not retained.
/// Keep query storage clear of the initialized scratch stack's 144-byte peak
/// reservation. Releases scratch storage on every exit and changes GTE state.
s32 worldCollisionSegmentOccluded(const SVECTOR* segmentStart, const SVECTOR* segmentEnd);

/// Grid-contact presence and opposed-normal status returned by pushback resolvers.
enum {
    WORLD_COLLISION_PUSHBACK_NO_GRID_HIT = 0,
    WORLD_COLLISION_PUSHBACK_GRID_HIT    = 1,
    WORLD_COLLISION_PUSHBACK_OPPOSED     = 2
};

/// Sums grid pushback and adds the average vertical correction from floor normals.
///
/// Reads exactly `contactCount` elements, ignoring LAST and non-grid contacts.
/// Supply a nonnegative count no greater than 32768, live readable contacts,
/// and disjoint writable outputs. Zero count returns NO_GRID_HIT and leaves
/// both outputs untouched. Otherwise XYZ in `delta` is always written as signed
/// 16.16 room-space correction.
/// Normals use 4096 per unit and distances use game units. Normals with Y below
/// -3546 contribute only Y, averaged with signed division before conversion;
/// other enabled responses are summed without averaging or response-kind dispatch.
///
/// `surfaceMaskOut` may be NULL; otherwise it receives the OR of `1 << key`
/// over every occupied grid contact, including suppressed surfaces. Target
/// shifts use key bits 0..4; valid grid surface classes are 0..7. Suppression
/// flags must be loaded for the active room. Returns GRID_HIT for any occupied
/// grid contact, even with zero correction, or OPPOSED when two enabled
/// non-floor normals oppose strongly on X or Z. OPPOSED still writes the delta.
/// Uses a count-sized signed-halfword index array on the C stack and 52 bytes
/// on the initialized scratch stack, disjoint from inputs and outputs. Releases
/// scratch storage before return and retains no pointers.
s32 worldCollisionResolvePushback(const WorldCollisionContact* contacts, WorldCollisionDelta* delta, s32 contactCount, s32* surfaceMaskOut);

/// Resolves grid overlap, floor-lift and edge-slide contacts into one correction.
///
/// Reads exactly `contactCount` elements, in 0..32, ignoring LAST and non-grid
/// contacts. Supply live readable contacts and disjoint writable outputs.
/// Zero count returns NO_GRID_HIT without touching either output. Otherwise
/// XYZ in `delta` is always written as signed 16.16 room-space correction.
/// Normals use 4096 per unit and distances use game units. Suppression flags
/// must be loaded for the active room; suppressed surfaces provide no correction.
///
/// Response 0 sums normal times distance; response 1 keeps the last floor's
/// negative Y lift; response 2 keeps the nearest horizontal edge correction.
/// Other response values provide no correction. Zero is the edge-distance
/// sentinel, so a selected zero-distance edge can be replaced by a later edge.
/// Any enabled response-0 contact selects overlap plus floor; otherwise the
/// result is floor plus edge. Signed word arithmetic and narrowing are retained.
///
/// `surfaceMaskOut` may be NULL; otherwise it receives the OR of `1 << key`
/// over every occupied grid contact, including suppressed and unknown responses.
/// Target shifts use key bits 0..4; valid grid surface classes are 0..7.
/// Returns GRID_HIT for any occupied grid contact, even with zero correction,
/// or OPPOSED for strongly opposed enabled response-0 normals on X or Z.
/// OPPOSED still writes the delta. Uses 32 byte-sized indices on the C stack
/// and 64 bytes on the initialized scratch stack, clear of inputs and outputs.
/// Releases scratch storage before return and retains no pointers.
s32 worldCollisionResolveResponsePushback(const WorldCollisionContact* contacts, WorldCollisionDelta* delta, s32 contactCount, s32* surfaceMaskOut);

/// Applies response-dispatched grid correction to a coordinate's local translation.
///
/// Reads `contactCount` contacts in 0..32, with the contracts of
/// `worldCollisionResolveResponsePushback`; current callers pass six. Steps
/// each fractional 16.16 component one whole unit away from zero, then adds
/// its signed integer half to XYZ. Negative fractions therefore take one
/// further negative unit beyond the integer half's flooring. Translation and correction must share a coordinate
/// frame; fixed-value rounding and resulting coordinates must fit their storage.
/// Does not invalidate or compose the node.
///
/// Returns NO_GRID_HIT when no grid hit occurs or the resolved XZ correction
/// is zero, otherwise GRID_HIT or OPPOSED; vertical correction still applies
/// when the return is zero. On a grid hit, non-NULL `surfaceClassOut` receives
/// the class selected from the collected surface mask. With no grid hit it is
/// left untouched. Inputs, coordinate and optional output must be live and
/// disjoint from the initialized scratch stack; no pointers are retained.
s32 worldCollisionApplyResponsePushback(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s32* surfaceClassOut);

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

/// Runtime trigger lists; list membership selects the collision test and hit consumer.
enum {
    WORLD_COLLISION_TRIGGER_LIST_ACTION          = 0,
    WORLD_COLLISION_TRIGGER_LIST_VIEW_BOUNDARIES = 1
};

/// Appends a borrowed trigger to the selected action or view-boundary list.
///
/// `listIndex` is WORLD_COLLISION_TRIGGER_LIST_ACTION or
/// WORLD_COLLISION_TRIGGER_LIST_VIEW_BOUNDARIES. A non-NULL trigger already
/// marked LINKED is left unchanged. Otherwise linking sets LINKED and installs
/// both links, preserving insertion order, other flags and the hit latch.
/// The owner keeps the trigger and its bound transform alive until unlinking;
/// initialize geometry and explicitly enable the trigger before collision scans.
void worldCollisionLinkTrigger(s32 listIndex, WorldCollisionTrigger* trigger);

/// Unlinks a borrowed trigger, retaining only its kind and resource LAST marker.
///
/// `unusedListIndex` is ignored; the incoming link identifies the actual list.
/// A linked trigger must have valid predecessor and successor links. Repairs
/// those links before clearing its own, without freeing storage or changing
/// geometry, transform or hit latch. Unlinked triggers are unchanged; pass
/// enables must be restored before a removed trigger is used again.
void worldCollisionUnlinkTrigger(s32 unusedListIndex, WorldCollisionTrigger* trigger);

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

/// Reads the first latched action trigger and enables subsequent hit clearing.
///
/// Returns 1 for the first hit in insertion order and writes its control word
/// and both full, selector-dependent parameter bytes to non-NULL, disjoint
/// outputs. See `WorldCollisionTrigger` for each selector's interpretation.
/// Returns 0 without writing outputs when no hit exists. Reading preserves
/// every latch, so repeated reads can return the same trigger before the next
/// collision tick. Once a hit is read, action latches are cleared before each
/// subsequent trigger scan until `worldCollisionResetListsAndGrid` resets the
/// clearing gate. Outputs are borrowed only for this call.
s32 worldCollisionReadActionHit(u16* controlOut, u8* parameter0Out, u8* parameter1Out);

#endif // GAMEPLAY_WORLD_COLLISION_H
