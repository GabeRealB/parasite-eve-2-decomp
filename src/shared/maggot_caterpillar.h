/* The code the Maggot (actor_102600) and the Caterpillar (actor_105500) share;
 * the code sees the work block as Actor105500Work. It waits until the player
 * comes near, then either drops from above on a line, drawn fading grey, or
 * makes a scripted entrance. After that it turns toward the player and
 * chooses an attack. Close up it pounces forward, stepping along a per-frame
 * stride table; if the pounce hits something it bounces back. If it is not
 * burning and the player is not blinded, it instead sprays a burst of short-
 * lived puff projectiles, each a growing textured sprite. A type-7 hit sets it
 * burning: it gives off an effect from two body nodes, plays a periodic sound
 * and its pounce does elemental damage. Timed status hits damage it, a type-2
 * hit stuns it, and when it dies it collapses, squashes flat and fades,
 * leaving a husk model lit with the room's texture page.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAGGOT_CATERPILLAR_H
#define SRC_SHARED_MAGGOT_CATERPILLAR_H

#include "types.h"

#include "main/task_types.h"

void maggotCaterpillarSprayState(Task* arg0);
void maggotCaterpillarPounceState(Task* arg0);
void maggotCaterpillarHurtState(Task* arg0);
void maggotCaterpillarEntranceState(Task* arg0);
void maggotCaterpillarBurnStep(Task* arg0);
void maggotCaterpillarTurnStep(Task* arg0);
void maggotCaterpillarDyingState(Enemy* arg0, Task* arg1);
void maggotCaterpillarPuffTick(Enemy* arg0, Task* arg1);
void maggotCaterpillarDrawPuff(Task* actor, s32 frame);
void maggotCaterpillarDrawThread(Task* actor);
void maggotCaterpillarTick(Enemy* arg0, Task* arg1);
void maggotCaterpillarApplyStatus(Task* arg0);
void maggotCaterpillarStunState(Task* arg0);
void maggotCaterpillarMoveStep(Task* arg0);
void maggotCaterpillarDrawShadow(Task* arg0);
void maggotCaterpillarSquash(Task* arg0);
void maggotCaterpillarSpawnHusk(Task* actor);
void maggotCaterpillarShrinkNode2(Task* actor);
void maggotCaterpillarPuffSetup(Enemy* enemy, Task* task);

/* Defined by each package. */
void maggotCaterpillarResolveContacts(Task* arg0);
void maggotCaterpillarRunBehaviour(Task* arg0);
void maggotCaterpillarTickAnim(Task* arg0);
void maggotCaterpillarUpdateColor(Task* arg0);

static inline void maggotCaterpillarTickAnimInline(Task* task);

#endif /* SRC_SHARED_MAGGOT_CATERPILLAR_H */
