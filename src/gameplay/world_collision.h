#ifndef GAMEPLAY_PRIVATE_WORLD_COLLISION_H
#define GAMEPLAY_PRIVATE_WORLD_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"

// Collision lists, contact records, room grids and collision updates.

/// Pair-handler table used by `_worldCollisionCollideListPairs` / `worldCollisionCollideBodyLists`.
/// Indexed by `WorldCollisionPairRule::handlerIndex` (`worldCollisionPairNop` / `_worldCollisionCollideSpherePair` /
/// `_worldCollisionCollideSphereCapsulePair`).
extern WorldCollisionPairHandler Gp_PairHandlers[5];

/// Pair-rule table used by `_worldCollisionCollideListPairs` / `worldCollisionCollideBodyLists`, one rule
/// per ordered pair of body kinds. Rows and columns are `(flags & 7) - 1`.
extern WorldCollisionPairRule D_8010FA4C[4][4];

/// Set to 1 by `worldCollisionReadActionHit` when a pending `Gp_PendingObj4C` node is found;
/// `Gp_TickWorldCollision` then calls `worldCollisionClearActionHits` to clear those flags.
extern s32 Gp_PendingObj4CFlag;

/// Appends grid-face overlap contacts for an ordinary sphere's centre cell.
///
/// Requires a kind-1 body with a live composed transform, writable initialized
/// LAST-terminated contacts and an active grid with a composed view transform.
/// Both transforms must use the same query frame; geometry indices and cell
/// face lists must be valid, with CELL_END terminating each non-NULL list.
/// Tests only the centre's cell, ignoring out-of-range cells and disabled faces
/// whose first two vertex indices are zero. Normals use 4096 per unit; edges
/// must be nonzero with signed-halfword deltas meeting SDK normalization bounds.
///
/// Accepts either side of a face within the sphere radius and 10 game units
/// of outward edge slack. Plane distances narrow to signed halfwords. Each
/// accepted face takes the first free slot, preserving flags and setting
/// OCCUPIED; exhaustion stops the walk. Writes radius minus signed plane
/// distance, the surface/grid key, a zero point and the original grid normal.
/// Existing contacts remain, including repeated normals. Direct calls bypass
/// GRID_ENABLED. Storage must be clear of the initialized scratch stack's
/// 136-byte block and nested queries. Releases its block on every exit,
/// changes GTE state and retains no pointers.
void worldCollisionCollideSphereGrid(const WorldCollisionBody* body);

/// Records direction-filtered grid overlaps for a motion sphere's centre cell.
///
/// Requires a kind-4 body and live motion context with a writable initialized
/// LAST-terminated contact table. Body, direction and active grid must share
/// the cached transforms' query frame. Cell lists and triangle/quad geometry
/// meet worldCollisionCollideSphereGrid's bounds. Tests only the centre cell;
/// out-of-range cells, NULL lists and disabled faces produce no new contacts.
/// Direct calls bypass GRID_ENABLED and perform no separate floor query.
///
/// Rejects grid-normal Y below -3546 (4096 per unit) and direction/rotated-normal
/// dot products above 2621440 (24 fractional bits). Requires face-plane overlap
/// within the radius. Outward edge slack is the radius; a centre outside an
/// edge also needs nonnegative signed face distance and gets GRID_EDGE.
/// Plane distances narrow to signed halfwords. Matching grid/edge contacts
/// with the same original normal keep the larger penetration and the earlier
/// surface key; otherwise fills the first free entry with a zero point and
/// original grid normal. Exhaustion stops the scan. Reserves/releases a
/// 136-byte scratch block around nested queries, changes GTE state and retains
/// no pointers. All borrowed storage must be live and clear of that stack.
void worldCollisionCollideMotionSphereGrid(const WorldCollisionBody* body);

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

/// Records crossed floor faces in contact zero of a motion sphere.
///
/// Requires a kind-4 body's live motion context and at least one writable,
/// initialized contact. The active grid must have at most 256 faces, valid
/// cells/mesh indices and unit room normals using 4096 per unit. Body and view
/// transforms must already be composed; candidate-footprint and floor-segment
/// placement contracts apply. Tests normals strictly below
/// WORLD_COLLISION_FLOOR_NORMAL_Y, roughly within 30 degrees of -Y.
///
/// An empty contact receives OCCUPIED and a GRID_FLOOR/surface key. An occupied
/// key is replaced only by a greater unsigned surface class; its other flags
/// survive. Point, original grid normal and distance in game units are replaced
/// for each hit, independently of that key. Surface classes must be in 0..7.
/// Distance is from the initially placed endpoint 0; each hit clips that end
/// for subsequent tests. No hit leaves the contact untouched; LAST is preserved.
/// Hit-distance squared sums must fit a nonnegative signed word for SquareRoot0.
/// Direct calls bypass GRID_ENABLED and FLOOR_QUERY. Clears the shared face
/// mask, changes GTE state and retains no pointers. Borrowed inputs and contact
/// storage must stay clear of the initialized scratch stack's 200-byte peak
/// reservation, including nested queries; releases its block before return.
void worldCollisionQueryMotionSphereFloor(const WorldCollisionBody* body);

/// Records directed grid-face crossings for a capsule body.
///
/// Requires a kind-3 body with a live capsule, composed body/view transforms
/// in the same query frame and an active grid with valid mesh/cell indices.
/// Face count must fit the 256-byte candidate mask; normals use 4096 per unit.
/// Segment placement and face-intersection normalization bounds apply.
/// Surface classes index the current stage/area's surface table in 0..7.
///
/// Contacts are writable and initialized, ending with LAST. A CLIP_TO_GRID_CONTACT
/// body needs contact zero: each probe-blocking hit replaces it and shortens
/// endpoint zero, retaining the nearest crossing. Other bodies append every
/// crossing to the first free slot, stopping when the LAST slot is filled or
/// encountered with flags exactly OCCUPIED | LAST. Existing entries survive.
/// An occupied LAST entry must have no additional flag bits to stop the scan.
/// Writes a GRID/surface key, zero distance, the original room normal and a
/// signed-halfword hit point in the cached composition frame (normally view
/// space). Contact flags survive with OCCUPIED added; no hit leaves them intact.
///
/// Direct calls bypass GRID_ENABLED. Clears the shared candidate mask and
/// changes GTE state. Borrowed storage must stay clear of the initialized
/// scratch stack's 168-byte peak reservation; releases it and retains no pointers.
void worldCollisionCollideCapsuleGrid(const WorldCollisionBody* body);

/// Contact-seeding policies for capsule segment placement; any nonzero mode selects the grid rule.
enum {
    WORLD_COLLISION_CAPSULE_SEGMENT_PAIR_TEST = 0,
    WORLD_COLLISION_CAPSULE_SEGMENT_GRID_SCAN = 1
};

/// Places a capsule's segment in its cached transform frame and returns its axis.
///
/// Requires a kind-3 body with a live capsule and already composed coord->workm.
/// Adds pos to each local endpoint, narrowing to signed halfwords, then rotates
/// and translates it into that transform's composition frame in game units.
/// Writes two endpoint XYZs and endpoint 0 minus endpoint 1 normalized to 4096
/// per unit in direction. Output pad components are untouched.
///
/// In PAIR_TEST mode, SINGLE_CONTACT seeds endpoint 0 from the first occupied
/// contact of any kind. Only without SINGLE_CONTACT does CLIP_TO_GRID_CONTACT
/// seed it from the first occupied grid contact. GRID_SCAN (or any nonzero
/// gridScan) ignores SINGLE_CONTACT and applies only CLIP_TO_GRID_CONTACT.
/// A seeded point is already in the output frame; only endpoint 1 is transformed.
/// Contact tables must be live and LAST-terminated when either rule is selected.
///
/// The final delta must fit signed halfwords and have squared length in
/// 1..0x7FFFFFFF for SDK normalization. Outputs, body and borrowed inputs must
/// be disjoint from each other and the initialized scratch stack's 24-byte
/// reservation. Releases it before return, changes GTE state, retains no pointers.
void worldCollisionPlaceCapsuleSegment(const WorldCollisionBody* body, VECTOR endpoints[2], SVECTOR* direction, s32 gridScan);

/// Latches an action trigger when a body's sphere meets its proximity/facing and quad gates.
///
/// Both cached transforms must place geometry in the same query frame, in game
/// units; normals/directions use 4096 per unit. Radius-sum equality is accepted.
/// FACING_QUAD requires the body's local +Z axis to oppose facingNormal;
/// NEAR_OR_FACING_QUAD accepts centre distance below 500 after broad phase, or
/// requires its composed +Z axis to face the leveled origin before the quad test.
/// Other kinds run the quad test directly. It accepts only the plane's negative
/// side within body radius and a centre strictly inside all four edges; the
/// plane offset narrows to a signed halfword. Success writes hit = 1; every
/// rejection preserves the existing latch. Enable/list flags are caller gates.
/// Requires live body/trigger/transforms clear of the initialized scratch stack's
/// 200-byte peak reservation and SDK-valid normalization inputs. Releases all
/// scratch storage, changes GTE state and retains no pointers.
void worldCollisionTestActionTriggerSphere(const WorldCollisionBody* body, WorldCollisionTrigger* trigger);

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
/// `Gp_TickWorldCollision` runs `worldCollisionCollideBodyListGrid` over each list and
/// `worldCollisionCollideBodyLists` over the pairs that can interact.
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

/// Dispatches enabled body contacts for every ordered pair from two lists.
///
/// NULL heads are allowed. Enabled bodies must have kinds SPHERE through
/// MOTION_SPHERE (1..4), selecting the 4-by-4 pair-rule matrix. Each rule chooses
/// the handler and argument order; the handler result is ignored. Lists must be
/// acyclic and remain live and structurally unchanged during dispatch. Handler
/// contact-buffer, transform and scratch requirements apply to each body.
/// Lists should be disjoint when self-pairs or repeated contacts are unwanted.
void worldCollisionCollideBodyLists(WorldCollisionBody* firstList, WorldCollisionBody* secondList);

/// Dispatches grid contact tests for enabled bodies on one collision list.
///
/// A NULL head or absent active grid does nothing. NONE, CONTACT_PROXY and
/// unknown kinds have no handler. Sphere, capsule and motion-sphere bodies
/// need their live kind-specific contexts, composed transforms and writable
/// initialized contact tables; each handler's grid/scratch bounds apply.
/// Motion spheres with FLOOR_QUERY run their floor query before overlap tests.
/// The list must be acyclic, live and structurally unchanged throughout the walk.
/// Contacts are accumulated without clearing existing entries or relinking bodies.
void worldCollisionCollideBodyListGrid(const WorldCollisionBody* body);

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
