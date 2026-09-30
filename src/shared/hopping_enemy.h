/* Shared code of one hopping enemy species (actor_04400, actor_341700 and
 * actor_342400): it twists its spine by a third of the turn per joint, links
 * its three collision spheres, pins a model part to a world position and
 * starts its death squash. The two later packages also share a creep-forward-
 * until-hit state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The code sees the species' work block as Actor341700Work. The
 * later two packages name the creep state twice in their state table; the
 * second entry includes the fragment again under its own name.
 */

#ifndef SRC_SHARED_HOPPING_ENEMY_H
#define SRC_SHARED_HOPPING_ENEMY_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

void hopperTwistSpine(Task* arg0);
void hopperLinkBodies(Task* arg0);
void hopperPinPart(Task* arg0, s16 part, SVECTOR3* pos);
void hopperBeginDeath(Task* arg0);
void hopperCreepUntilHit(Task* arg0);

void hopperStartLeap(Task* arg0);
void hopperStartHold(Task* arg0);
void hopperStartAlert(Task* arg0);
void hopperBurst(Task* arg0);
void hopperDropBodies(Task* arg0);
void hopperBeginShrink(Task* task);

/* Defined by each package. */
void hopperSpawnGibs(Task* arg0);

#endif /* SRC_SHARED_HOPPING_ENEMY_H */
