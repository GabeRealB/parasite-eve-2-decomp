/* The movement core of the Boss Stranger (cafeteria_boss_stranger, and the Acropolis
 * bridge room's copy of it), which walks a patrol network of nav nodes. Each
 * tick it either heads for a player position, re-plans along `nodeOrder`
 * towards the player (nearest node to the player vs nearest node to itself,
 * pairing the closest slots of that order), or cycles its fixed patrol route.
 * It then turns towards the goal by a capped rate, ramps its speed, and steps
 * along its facing. It also resolves its collision/gravity delta and pushes
 * itself away from nearby contacts.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BOSS_STRANGER_H
#define SRC_SHARED_BOSS_STRANGER_H

#include "types.h"

#include "actors/actor.h"

#include "gameplay/geometry.h"

#include "engine/wipsys_types.h"

/// One node of the Boss Stranger's patrol table: a world position the walker
/// steers toward.
///
/// The coordinates are signed game coordinates. Code that copies a live
/// translation keeps its low halfword, and the arrival and nearest-node tests
/// subtract that same halfword. The trailing bytes are never read; they make
/// each node eight bytes.
typedef struct {
    s16  x;          // World X, in signed game-coordinate units
    s16  y;          // World Y, in signed game-coordinate units
    s16  z;          // World Z, in signed game-coordinate units
    byte pad_6[0x2]; // Unread; makes each node eight bytes
} BossStrangerNode;
STATIC_ASSERT_SIZEOF(BossStrangerNode, 0x8);

/// Nav record of a Boss Stranger patrol walker: the positions it can steer
/// toward, and the order of node indices the close-in state walks.
///
/// `nodeCount` is how many leading entries of `nodes` the nearest-node scans
/// walk. It is not the allocated length of that table: a patrol route may
/// name a later entry. `nodeOrder` is a second index list, distinct from the
/// route. It holds `orderCount` node indices and has no end marker. The
/// walker's `cursor` indexes it, and the re-plan searches it for the steps
/// that name two nodes.
typedef struct {
    BossStrangerNode* nodes;      // Positions the walker steers toward
    u8*               nodeOrder;  // Node index at each step. The walker's cursor indexes this
    u8                nodeCount;  // Leading nodes the nearest-node scans walk
    u8                orderCount; // Live entries in nodeOrder
    byte              pad_A[0x2]; // Unread. Keeps the record twelve bytes
} BossStrangerNav;
STATIC_ASSERT_SIZEOF(BossStrangerNav, 0xC);

/// Byte sentinel that wraps a Boss Stranger patrol route to its first node.
///
/// Reserved from navigation-node indices. A nonempty route starts with a real
/// index; arrival advances its byte cursor and tests the following entry for
/// this value before reading a node. The marker repeats the route, not a stop.
#define BOSS_STRANGER_ROUTE_END 0xFF

/// One Boss Stranger patrol route: the order of nav-node indices the walker
/// follows, and the cursor into that order.
///
/// `nodeIndices` runs until `BOSS_STRANGER_ROUTE_END`. On the frame the
/// walker reaches the node it is heading for, `arrived` is raised and `cursor`
/// advances, returning to the first index at the end marker.
typedef struct {
    u8*  nodeIndices; // Indices into the walker's nav node table, ended by BOSS_STRANGER_ROUTE_END
    u8   field_4;     // Role unproven. Actor spawn stores 2 and the route rebuild stores 0; nothing reads it
    u8   cursor;      // Index of the current entry. Wraps to 0 at the end marker
    u8   arrived;     // 1 on the frame the current node is reached, otherwise 0
    byte pad_7[0x1];  // Unread. Pointer alignment rounds the route to eight bytes
} BossStrangerRoute;
STATIC_ASSERT_SIZEOF(BossStrangerRoute, 0x8);

/// Steering modes stored in the byte `BossStrangerWalker::state`.
enum {
    /// Skips goal selection while the speed ramp and movement still run.
    ///
    /// Turning still uses the unwritten tick goal. Idle callers disable turning
    /// through turnLimit; this state alone does not stop the current speed.
    BOSS_STRANGER_WALKER_IDLE = 0,

    /// Steers directly toward the player selected by the walker's playerId.
    ///
    /// Copies the player's current translation into the signed-halfword goal.
    /// playerId is one-based; only player 1 has established resident storage.
    BOSS_STRANGER_WALKER_CHASE = 1,

    /// Plans toward player 1 along the navigation node order.
    ///
    /// Neither known carrier selects this mode. It advances the order cursor on
    /// arrival but leaves the tick goal unwritten. Its extra four scratch bytes
    /// are released only on arrival; a non-arriving tick leaves the cursor lower.
    BOSS_STRANGER_WALKER_CLOSE = 2,

    /// Steers along the looping patrol route of navigation-node indices.
    ///
    /// Arrival selects the next index, wraps at `BOSS_STRANGER_ROUTE_END`, and
    /// raises route->arrived for that tick before writing the new steering goal.
    BOSS_STRANGER_WALKER_PATROL = 3
};

/// Movement record of the Boss Stranger walker.
///
/// Embedded in `cafeteria_boss_stranger` and the Acropolis bridge copy. Each tick steers
/// `coord` toward the selected player, along `nav`'s node order, or along the
/// patrol route, then ramps `speed` and turns within `turnLimit`. The ground
/// step and contact avoidance run unless the carrier skips them. `scaleMtx`
/// keeps the model's saved rotation and is rebuilt around `scale`.
typedef struct {
    BossStrangerNav*       nav;           // Node table and order. Usually &navData
    BossStrangerRoute*     route;         // Patrol route. Usually &routeData
    GfxCoord*              coord;         // Coordinate the step writes
    WorldCollisionContact* recs;          // Contacts the ground step measures. NULL on the bridge
    WorldCollisionContact* avoidRecs;     // Contacts the avoidance step backs away from
    byte                   pad_14[0x8];   // Unread
    SVECTOR                moveStep;      // Facing step applied this frame, or zero while actors are frozen
    SVECTOR                moveDelta;     // Whole-unit collision step the ground step applied
    SVECTOR3               push;          // Sum of the avoidance nudges this frame
    byte                   pad_32[0x2];   // Aligns scaleMtx
    MATRIX                 scaleMtx;      // Saved rotation the turn step copies back before applying yaw
    s16                    scale;         // Uniform model scale. 0x1000 is full size
    s16                    recCount;      // Contact count passed with recs. The actor stores 12, the bridge 0
    s16                    avoidCount;    // Leading avoidRecs the avoidance step walks
    u16                    turnLimit;     // Maximum yaw change per frame, in angle units. 0 forces the turn to 0
    u16                    speedTarget;   // Unsigned ramp target. 0xFFFE is the patrol-out sentinel, applied as a step of -2
    u16                    speed;         // Current speed, applied as a signed step along facing
    u16                    speedStep;     // Most that speed may change in one frame
    u16                    turnRun;       // Consecutive frames the relative bearing was nonzero. Cleared at 0 and on route arrival
    u16                    turnBonus;     // Added to turnLimit. Each duration tier stores 0, and route arrival clears it
    byte                   pad_66[0x2];   // Unread
    u8                     state;         // BOSS_STRANGER_WALKER_IDLE, CHASE, CLOSE or PATROL
    u8                     prevState;     // State the previous close-in tick ran. A change re-plans
    u8                     node;          // Nav-node index being steered toward
    u8                     lockHeight;    // Non-zero pins Y: the ground step discards collision Y and skips the fall
    u8                     skipGround;    // Non-zero skips the ground step
    u8                     skipAvoid;     // Non-zero skips contact avoidance
    u8                     playerId;      // One-based player-status selector, copied from the save character id. Chase uses playerId - 1. Only initialization is known to store 1; a second resident record is not established
    u8                     actorNode;     // Nav node nearest the selected actor
    u8                     selfNode;      // Nav node nearest this walker
    u8                     prevActorNode; // Previous actorNode. A change re-plans the close-in
    u8                     prevSelfNode;  // Previous selfNode. A change re-plans the close-in
    s8                     orderStep;     // +1 or -1 along nodeOrder. Added to cursor through a u8 cast, so it wraps
    byte                   pad_74[0x1];   // Unread
    u8                     goalSlot;      // nodeOrder slot on the actor side of the closest pair. Nothing reads it
    u8                     cursor;        // Index into nav->nodeOrder while closing in
    u8                     blocked;       // 1 when an avoidance contact's kind is 0x10000. Nothing reads it
    u8                     offOrigin;     // 1 when local X or Z is nonzero after the ground step. Nothing reads it
    byte                   pad_79[0x7];   // Unread
    BossStrangerNav        navData;       // Storage nav usually points at
    BossStrangerRoute      routeData;     // Storage route usually points at
} BossStrangerWalker;
STATIC_ASSERT_SIZEOF(BossStrangerWalker, 0x94);

/// Unsigned-word sentinel for a nearest-node scan with no selected node.
///
/// Both scans compare unsigned squared X/Z distances formed from signed
/// halfword offsets. Their largest sum is 0x80000000, below this sentinel;
/// the explicit sentinel test also guarantees selection of the first node.
#define BOSS_STRANGER_NODE_DISTANCE_NONE 0xFFFFFFFFU

/// Scratch-pad block of the scan for the nav node nearest the walker itself.
///
/// Each node of the walker's table is measured against the translation of the
/// walker's own coordinate, using the low 16 bits of each axis. The distance
/// is taken on the XZ plane and no height offset is staged: the halfword
/// between `dx` and `dz` is never accessed. `nearest` has no value when the
/// node table is empty.
typedef struct {
    s16  dx;          // Walker X minus the node's
    byte pad_2[0x2];  // Never accessed. The player scan's block keeps its unread height offset here
    s16  dz;          // Walker Z minus the node's
    byte pad_6[0x2];  // Unread. Aligns bestDistSq
    u32  bestDistSq;  // Smallest distSq taken so far, or BOSS_STRANGER_NODE_DISTANCE_NONE
    u32  distSq;      // Squared XZ distance from the node under test to the walker
    u8   node;        // Index of the node under test
    u8   nearest;     // Index of the node bestDistSq was measured at. The scan's result
    byte pad_12[0x2]; // Unread. Rounds the block up to a whole word
} BossStrangerNodeNearestSelfScratch;
STATIC_ASSERT_SIZEOF(BossStrangerNodeNearestSelfScratch, 0x14);

/// Scratch-pad block of the scan for the nav node nearest a player.
///
/// Each node of the walker's table is measured against the root coordinate of
/// one player status record, using the low 16 bits of each axis. The distance
/// is taken on the XZ plane, so the height offset is staged and never read.
/// `nearest` has no value when the node table is empty.
typedef struct {
    s16           dx;          // Player X minus the node's
    s16           dy;          // Player Y minus the node's. Stored, never read
    s16           dz;          // Player Z minus the node's
    byte          pad_6[0x2];  // Unread. Aligns player
    PlayerStatus* player;      // Status record whose root coordinate the scan measures from
    u32           bestDistSq;  // Smallest distSq taken so far, or BOSS_STRANGER_NODE_DISTANCE_NONE
    u32           distSq;      // Squared XZ distance from the node under test to the player
    u8            node;        // Index of the node under test
    u8            nearest;     // Index of the node bestDistSq was measured at. The scan's result
    byte          pad_16[0x2]; // Unread. Rounds the block up to a whole word
} BossStrangerNodeNearestPlayerScratch;
STATIC_ASSERT_SIZEOF(BossStrangerNodeNearestPlayerScratch, 0x18);

/// Scratch-pad block of the walker's ground step.
///
/// Contact resolution writes the push-back from the walker's measured contacts
/// into `delta`. The step then takes each axis in whole units into `move`,
/// adds the fall, saves the result in the walker and applies it to the
/// walker's coordinate. Nothing clears the block when it is reserved.
typedef struct {
    WorldCollisionDelta delta; // Push-back in signed 16.16 units. Left unwritten when the walker measures no contacts
    SVECTOR             move;  // Whole-unit step for this frame. A resolved, unlocked Y adds onto the value the reservation found here, and pad is never written
} BossStrangerGroundStepScratch;
STATIC_ASSERT_SIZEOF(BossStrangerGroundStepScratch, 0x18);

/// Number of order-slot bytes in each re-plan list, including marker storage.
///
/// Collection accepts eight occurrences, but then writes the end marker at
/// the count. Valid orders therefore have at most seven occurrences per node;
/// retain the full-list overrun described by `BossStrangerPlanTowardScratch`.
enum { BOSS_STRANGER_PLAN_SLOT_CAPACITY = 8 };

/// Byte end marker for either collected list of navigation-order slots.
///
/// The byte order count limits valid indices to 0..254. The marker is stored
/// at list[count], and pairing tests it before reading a pair's slot values.
enum { BOSS_STRANGER_PLAN_SLOT_END = 0xFF };

/// Byte sentinel for a re-plan with no selected pair of order slots.
///
/// Valid slot indices are 0..254, so their absolute gap is at most 254.
/// Pairing replaces this value on the first pair and retains the first tie.
enum { BOSS_STRANGER_PLAN_GAP_NONE = 0xFF };

/// Scratch-pad block of the walker's re-plan along its nav's node order.
///
/// The re-plan first collects the order slots naming the node nearest the
/// selected actor and those naming the node nearest the walker, then tests
/// every pair of one slot from each list and keeps the pair the fewest slots
/// apart. Nothing clears the block when it is reserved.
///
/// Each list takes at most `BOSS_STRANGER_PLAN_SLOT_CAPACITY` slots, in
/// ascending order, and later matches are dropped. The end marker is stored
/// at the list's count, so a list that fills has no room for it: the marker
/// of a full `selfSlots` replaces `actorSlots[0]`, and that of a full
/// `actorSlots` falls on the byte after the block. Neither carrier's order
/// names a node more than once.
typedef struct {
    s16  gap;                                          // Actor-side slot minus walker-side slot of the pair under test. Its sign picks the direction along the order
    byte pad_2[0x2];                                   // Never accessed
    u8   actorNode;                                    // Nav node nearest the selected actor
    u8   selfNode;                                     // Nav node nearest the walker
    u8   outerIndex;                                   // Order slot under test while collecting, then index into actorSlots while pairing
    u8   innerIndex;                                   // Index into selfSlots while pairing
    u8   bestGap;                                      // Smallest absolute gap taken so far, or BOSS_STRANGER_PLAN_GAP_NONE
    u8   actorSlotCount;                               // Slots collected in actorSlots
    u8   selfSlotCount;                                // Slots collected in selfSlots
    byte pad_B[0x1];                                   // Never accessed
    u8   selfSlots[BOSS_STRANGER_PLAN_SLOT_CAPACITY];  // Order slots naming selfNode, ended by BOSS_STRANGER_PLAN_SLOT_END unless full
    u8   actorSlots[BOSS_STRANGER_PLAN_SLOT_CAPACITY]; // Order slots naming actorNode, ended by BOSS_STRANGER_PLAN_SLOT_END unless full
} BossStrangerPlanTowardScratch;
STATIC_ASSERT_SIZEOF(BossStrangerPlanTowardScratch, 0x1C);

/// Scratch-pad block of the walker's turn step.
///
/// The step works on a single angle, in units of 4096 to the turn. It is
/// first the bearing of the goal relative to the walker's facing, wrapped to
/// [-0x800, 0x800], then that turn clamped to the walker's per-frame limit,
/// and finally the clamped turn added to the facing: the absolute yaw the
/// walker's rotation is rebuilt around. The goal's offset from the walker is
/// staged in a block of its own beneath this one, not in the bytes ahead of
/// `angle`. Nothing clears the block when it is reserved.
typedef struct {
    byte pad_0[0x18]; // Never accessed
    s16  angle;       // Relative bearing, then the clamped turn, then the absolute yaw
    byte pad_1A[0x2]; // Never accessed. Rounds the block up to a whole word
} BossStrangerTurnTowardScratch;
STATIC_ASSERT_SIZEOF(BossStrangerTurnTowardScratch, 0x1C);

/// Scratch-pad frame of the walker's tick.
///
/// The frame carries one value: the world position the walker turns towards
/// this frame. The chase state stores the low 16 bits of each axis of the
/// selected player's translation there and the patrol state the position of
/// the route's current nav node. The idle and close-in states store nothing,
/// and nothing clears the frame when it is reserved, so in those states the
/// turn step reads whatever the scratch pad last held there.
typedef struct {
    byte     pad_0[0x4];  // Never accessed
    SVECTOR3 goal;        // World position the turn step faces. Written by the chase and patrol states only
    byte     pad_A[0x1E]; // Never accessed
} BossStrangerTickScratch;
STATIC_ASSERT_SIZEOF(BossStrangerTickScratch, 0x28);

static s16  _bossStrangerArrived(const BossStrangerWalker* walker);
static void _bossStrangerFollowRoute(BossStrangerWalker* walker, SVECTOR3* goal);
static u8   _bossStrangerNodeNearestPlayer(const BossStrangerWalker* walker, s16 playerId);
static u8   _bossStrangerNodeNearestSelf(const BossStrangerWalker* walker);
static void _bossStrangerPlanToward(BossStrangerWalker* walker, s16 playerId);
static void _bossStrangerApplyGroundStep(BossStrangerWalker* walker);
static void _bossStrangerAvoidContacts(BossStrangerWalker* walker);
static void _bossStrangerTurnToward(BossStrangerWalker* walker, const SVECTOR3* goal);
static void _bossStrangerTick(BossStrangerWalker* walker);

#endif /* SRC_SHARED_BOSS_STRANGER_H */
