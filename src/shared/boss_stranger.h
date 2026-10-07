/* The movement core of the Boss Stranger (actor_110600, and the Acropolis
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

#include "overlay.h"

#include "gameplay/geometry.h"

#include "main/wipsys_types.h"

/// Value of `bestDistSq` in `BossStrangerNodeNearestSelfScratch` and
/// `BossStrangerNodeNearestPlayerScratch` before the scan has taken a node.
/// A real squared distance equal to it still replaces it, so the first node
/// tested always wins.
#define BOSS_STRANGER_NODE_DISTANCE_NONE 0xFFFFFFFF

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

/// Capacity of each slot list in `BossStrangerPlanTowardScratch`.
#define BOSS_STRANGER_PLAN_SLOT_CAPACITY 8

/// End marker of a slot list in `BossStrangerPlanTowardScratch`. No real slot
/// has this value: the order's length is a byte, so its slots end at 0xFE.
#define BOSS_STRANGER_PLAN_SLOT_END 0xFF

/// Value of `bestGap` in `BossStrangerPlanTowardScratch` before a pair has
/// been taken. Every real gap is smaller, so the first pair tested always
/// wins.
#define BOSS_STRANGER_PLAN_GAP_NONE 0xFF

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
