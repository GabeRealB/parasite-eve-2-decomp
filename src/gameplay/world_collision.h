#ifndef GAMEPLAY_PRIVATE_WORLD_COLLISION_H
#define GAMEPLAY_PRIVATE_WORLD_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"

// Collision lists, contact records, room grids and collision updates.

/// Pair-handler table used by `Gp_RunPairHandler` / `Gp_CollideLists`.
/// Indexed by `WorldCollisionPairRule::handlerIndex` (`worldCollisionPairNop` / `Gp_PairHandler1` /
/// `Gp_PairHandler3`).
extern WorldCollisionPairHandler Gp_PairHandlers[5];

/// Pair-rule table used by `Gp_RunPairHandler` / `Gp_CollideLists`, one rule
/// per ordered pair of body kinds. Rows and columns are `(flags & 7) - 1`.
extern WorldCollisionPairRule D_8010FA4C[4][4];

/// Set to 1 by `worldCollisionReadActionHit` when a pending `Gp_PendingObj4C` node is found;
/// `Gp_TickWorldCollision` then calls `worldCollisionClearActionHits` to clear those flags.
extern s32 Gp_PendingObj4CFlag;

void Gp_CollideObjGrid(WorldCollisionBody* arg0);

void Gp_CollideObjGridDir(WorldCollisionBody* arg0);

/// Tests a directed segment against one active-grid triangle or quad.
///
/// `faceIndex` is in [0, Gp_GridParams->faceCount); the grid, its valid mesh
/// indices and its composed `viewCoord->workm` must be live. `endpoints` holds
/// two query-space positions in game units. `directionAndHit[0]` is endpoint 0
/// minus endpoint 1, normalized with 4096 per unit in the same frame. A hit
/// requires endpoint 1 on the positive side of the face and a strictly negative
/// distance along that reversed direction from endpoint 0 to its plane.
/// Parallel and opposite-direction queries are rejected. The ray direction
/// must be nonzero; the face must have a unit normal and nonzero-length edges.
/// Edge deltas must fit signed halfwords with squared length in 1..0x7FFFFFFF,
/// as required by the SDK's GTE normalization.
///
/// Returns 1 when the plane hit passes every outward edge plane, else 0.
/// A non-NULL `bodyQuery` selects 10 game units of edge tolerance; NULL selects
/// 5 for a standalone probe. The body is never dereferenced. The face-plane
/// offset and edge distances are truncated to signed halfwords. The candidate
/// hit XYZ is written to `directionAndHit[1]`, also as signed halfwords, before
/// edge rejection, so a 0 result can leave a candidate hit. Its pad halfword,
/// the direction and the endpoints are untouched.
///
/// The output must not overlap the endpoints or mesh. All live query storage
/// must be disjoint from the initialized scratch stack's 112-byte reservation.
/// Releases that block on every exit, changes GTE rotation and arithmetic state,
/// and retains no pointers.
s32 worldCollisionIntersectGridFace(s32 faceIndex, const VECTOR endpoints[2], SVECTOR directionAndHit[2], const WorldCollisionBody* bodyQuery);

extern WorldCollisionTrigger* Gp_Obj4CList;

void func_800DD940(WorldCollisionBody* arg0);

void func_800DDDF8(WorldCollisionBody* obj);

void func_800DEC80(WorldCollisionBody* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3);

void func_800DEF80(WorldCollisionBody* node, WorldCollisionTrigger* other);

/// Latches a view boundary when a sphere overlaps its quad while moving against its normal.
///
/// `previousRootPosition` is the earlier root translation in the body's coordinate
/// parent frame, in game units. Current translation minus that position is
/// normalized to 4096 per unit and compared with the untransformed boundary normal.
/// Composed body and boundary matrices must place both in the same query space.
/// The sphere must reach the quad plane from its negative side, within its radius,
/// and its centre must lie strictly inside every edge. Plane offset narrows to a signed halfword.
/// The broad-phase radius test includes equality. Kind and enable flags are not
/// checked here. Success sets `boundary->hit` to 1; rejection retains its value.
///
/// Body, boundary, previous position and transforms must remain live and clear of
/// the initialized scratch stack's 224-byte peak reservation. Root displacement
/// must meet the SDK normalization's halfword and squared-length bounds. The
/// original discarded square-root call is retained. Releases all scratch storage,
/// changes GTE state and retains no pointers.
void worldCollisionTestViewBoundarySphere(const WorldCollisionBody* body, WorldCollisionTrigger* boundary, const VECTOR3* previousRootPosition);

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

/// Drops all body, trigger and occluder list bindings and the active room grid.
///
/// Also resets the action-hit clearing gate. Does not walk the old lists,
/// repair node links or clear LINKED, ENABLED, contacts or hit latches, and frees
/// no storage. Use at area-load teardown after old owners stop using the lists;
/// retained nodes must have their list state reinitialized before relinking.
void worldCollisionResetListsAndGrid(void);

/// Pair-dispatch callback for routes that perform no contact test.
///
/// Installed at handler indices 0, 2 and 4. Ignores both ordered bodies and
/// the handler index, changes no state, and returns 0 (no contact).
s32 worldCollisionPairNop(WorldCollisionBody* firstBody, WorldCollisionBody* secondBody, s32 handlerIndex);

/// Empties one trigger list and removes runtime list/pass state from its nodes.
///
/// `listIndex` selects ACTION or VIEW_BOUNDARIES on a live, acyclic list. Clears
/// every node's links and flags except its kind and resource LAST marker;
/// geometry, transform and hit latch survive. Storage remains owned by the
/// caller. Empty lists are unchanged.
void worldCollisionClearTriggerList(s32 listIndex);

/// Appends a borrowed sight occluder to the active list in insertion order.
///
/// `listIndex` must be 0, the sole occluder list. The non-NULL occluder and the
/// list head remain live until unlinking. Already-LINKED nodes are unchanged;
/// otherwise installs both links and sets LINKED, preserving all other flags
/// and geometry. The owner initializes the quad and sets ENABLED separately.
void worldCollisionLinkOccluder(s32 listIndex, WorldCollisionOccluder* occluder);

/// Empties the active sight-occluder list and clears each node's runtime state.
///
/// `listIndex` must be 0 and the list live and acyclic. Clears both links and
/// flag bits 3..6, preserving the uninterpreted low three bits, resource LAST
/// marker and all geometry. Does not free the borrowed records. An empty list
/// is unchanged.
void worldCollisionClearOccluderList(s32 listIndex);

/// Loads the current area's eight surface-class pushback suppression flags.
///
/// Requires a live session and loaded stage/area table with all eight non-NULL
/// surface records. Clears the active cache to APPLY_PUSHBACK, then copies each
/// record's full suppressPushback byte into its s32 cache entry. Grid response
/// queries use zero to enable correction and any nonzero value to suppress it.
void worldCollisionLoadSurfacePushbackFlags(void);

/// Consumes view-boundary latches and requests the last applicable destination view.
///
/// Every hit on the live, acyclic boundary list is cleared. A hit requests its
/// parameter1 destination in the live save only when parameter0 equals the
/// session's source view; that source remains unchanged throughout the walk,
/// so the last matching entry wins. Parameters are valid 1-based views in the
/// current room. No node is unlinked or freed, and no hit is required to run.
void worldCollisionConsumeViewBoundaryHits(void);

void Gp_CollideLists(WorldCollisionBody* a, WorldCollisionBody* b);

void Gp_CollideListGrid(WorldCollisionBody* node);

void func_800E0608(WorldCollisionBody* node, s32 mask, s32 match);

/// Tests enabled view boundaries against the first body matching a flags filter.
///
/// Compares `(body->flags & flagsMask)` with the low halfword of `flagsMatch`.
/// Matching bodies must be motion spheres with live composed transforms; the
/// player task must exist and its previousPosition supplies the movement origin
/// in the body's parent frame. Tests every enabled boundary on the live view
/// list, latching hits without clearing earlier ones. The boundary cursor is
/// initialized once and is exhausted by the first matching body; later matching
/// bodies receive no tests. Borrows all storage and changes GTE state.
void worldCollisionScanViewBoundaries(WorldCollisionBody* body, s32 flagsMask, s32 flagsMatch);

/// Converts a composed view-space position to signed room-grid XZ cell indices.
///
/// Requires the active grid, its composed view transform and positive cellSize.
/// Applies the transpose rotation, then subtracts the view coordinate's local
/// XZ translation and adds the grid biases before division in game units.
/// A negative biased axis produces WORLD_COLLISION_GRID_INVALID_CELL; a
/// nonnegative axis divides by cellSize and narrows to a signed halfword.
/// Y is set to zero and the output pad halfword is untouched. Upper bounds are
/// checked by callers, not clamped here. Input XYZ is unchanged. Storage must
/// be clear of the initialized scratch stack's 16-byte reservation, released
/// before return. Changes GTE state and retains no pointers.
void worldCollisionViewToCell(const VECTOR* viewPosition, SVECTOR* cellOut);

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

/// Clears every hit latch on the linked action-trigger list.
///
/// Leaves links, geometry, flags and the action-hit clearing gate unchanged.
/// The collision tick calls this before rescanning once an action hit has been
/// read; direct callers need only supply a live, acyclic list.
void worldCollisionClearActionHits(void);

#endif // GAMEPLAY_PRIVATE_WORLD_COLLISION_H
