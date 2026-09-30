/* The movement core of a creature that walks a patrol network of nav nodes,
 * used by actor_110600 and by the Acropolis bridge room's copy of it. Each
 * tick it either heads for a player position, re-plans along the route byte
 * table towards the player (nearest node to the player vs nearest node to
 * itself, pairing the closest table slots), or cycles its fixed patrol route.
 * It then turns towards the goal by a capped rate, ramps its speed, and steps
 * along its facing. It also resolves its collision/gravity delta and pushes
 * itself away from nearby contacts.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PATROL_WALKER_H
#define SRC_SHARED_PATROL_WALKER_H

#include "types.h"

#include "overlay.h"

s16  patrolArrived(OverlayWalker* walker);
void patrolFollowRoute(OverlayWalker* work, SVECTOR3* pos);
u8   patrolNodeNearestActor(OverlayWalker* work, s32 actor);
u8   patrolNodeNearestSelf(OverlayWalker* work);
void patrolPlanToward(OverlayWalker* work, s16 actor);
void patrolApplyGroundStep(OverlayWalker* work);
void patrolAvoidContacts(OverlayWalker* work);
void patrolTurnToward(OverlayWalker* work, SVECTOR3* pos);
void patrolWalkerTick(OverlayWalker* walker);

static inline void patrolWalkerStep(OverlayWalker* walker, u8* head, OverlayWalkerTickScratch* block);

#endif /* SRC_SHARED_PATROL_WALKER_H */
