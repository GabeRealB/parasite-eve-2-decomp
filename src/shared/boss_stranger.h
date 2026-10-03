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

#include "overlay.h"

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

s16  bossStrangerArrived(BossStrangerWalker* walker);
void bossStrangerFollowRoute(BossStrangerWalker* work, SVECTOR3* pos);
u8   bossStrangerNodeNearestActor(BossStrangerWalker* work, s32 actor);
u8   bossStrangerNodeNearestSelf(BossStrangerWalker* work);
void bossStrangerPlanToward(BossStrangerWalker* work, s16 actor);
void bossStrangerApplyGroundStep(BossStrangerWalker* work);
void bossStrangerAvoidContacts(BossStrangerWalker* work);
void bossStrangerTurnToward(BossStrangerWalker* work, SVECTOR3* pos);
void bossStrangerTick(BossStrangerWalker* walker);

static inline void bossStrangerStep(BossStrangerWalker* walker, u8* head, OverlayWalkerTickScratch* block);

#endif /* SRC_SHARED_BOSS_STRANGER_H */
