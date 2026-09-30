/* A spider-like enemy (work block Actor105500Work, sound bank 0x401A); the
 * species name is inferred from the code. It waits until the player comes
 * near, then either drops from above on a thread, drawn as a fading grey line,
 * or makes a scripted entrance. After that it turns toward the player and
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

#ifndef SRC_SHARED_WEB_SPIDER_H
#define SRC_SHARED_WEB_SPIDER_H

#include "types.h"

#include "main/task_types.h"

void spiderSprayState(Task* arg0);
void spiderPounceState(Task* arg0);
void spiderHurtState(Task* arg0);
void spiderEntranceState(Task* arg0);
void spiderBurnStep(Task* arg0);
void spiderTurnStep(Task* arg0);
void spiderDyingState(Enemy* arg0, Task* arg1);
void spiderPuffTick(Enemy* arg0, Task* arg1);
void spiderDrawPuff(Task* actor, s32 frame);
void spiderDrawThread(Task* actor);
void spiderTick(Enemy* arg0, Task* arg1);
void spiderApplyStatus(Task* arg0);
void spiderStunState(Task* arg0);
void spiderMoveStep(Task* arg0);
void spiderDrawShadow(Task* arg0);
void spiderSquash(Task* arg0);
void spiderSpawnHusk(Task* actor);
void spiderShrinkNode2(Task* actor);
void spiderPuffSetup(Enemy* enemy, Task* task);

/* Defined by each package. */
void spiderResolveContacts(Task* arg0);
void spiderRunBehaviour(Task* arg0);
void spiderTickAnim(Task* arg0);
void spiderUpdateColor(Task* arg0);

static inline void spiderTickAnimInline(Task* task);

#endif /* SRC_SHARED_WEB_SPIDER_H */
