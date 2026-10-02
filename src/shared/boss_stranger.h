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
