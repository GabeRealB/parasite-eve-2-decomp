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

s16  bossStrangerArrived(OverlayWalker* walker);
void bossStrangerFollowRoute(OverlayWalker* work, SVECTOR3* pos);
u8   bossStrangerNodeNearestActor(OverlayWalker* work, s32 actor);
u8   bossStrangerNodeNearestSelf(OverlayWalker* work);
void bossStrangerPlanToward(OverlayWalker* work, s16 actor);
void bossStrangerApplyGroundStep(OverlayWalker* work);
void bossStrangerAvoidContacts(OverlayWalker* work);
void bossStrangerTurnToward(OverlayWalker* work, SVECTOR3* pos);
void bossStrangerTick(OverlayWalker* walker);

static inline void bossStrangerStep(OverlayWalker* walker, u8* head, OverlayWalkerTickScratch* block);

#endif /* SRC_SHARED_BOSS_STRANGER_H */
