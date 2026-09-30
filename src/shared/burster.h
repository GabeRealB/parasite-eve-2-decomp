/* A crawling enemy that swells up and bursts next to the player. It spawns
 * either standing (dormant) or hidden until a message drops it into place from
 * a spawn point (maps 0x27/0x28). While dormant it rocks back and forth. When
 * the player enters its wide sensing sphere (a 0x10000 contact) it wakes,
 * turns toward the player by up to 0x20 a frame and crawls at 0x14 a step,
 * with a randomly timed idle sound. Within 0x320 of the player it swells, its
 * body-part scale growing 0xC8 a frame, and on the fifth frame it dies. The
 * death is picked at random (or forced): either it bursts - hit spheres armed,
 * burst effects, a death script, model hidden - or it slumps and its body is
 * flattened into the floor by a decaying Y scale before the enemy is
 * destroyed. It takes damage and critical kills from player hits (0x20000
 * contacts), is pushed out of walls (0x30000), and can be told by message to
 * collapse away (modes 4/5). A spawn-arg mode picks one of two sound banks and
 * an alternate texture page/CLUT.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BURSTER_H
#define SRC_SHARED_BURSTER_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

void bursterSpawnState(Enemy* arg0, Task* arg1);
void bursterReactionDispatch(Task* arg0);
void bursterDormantTick(Task* arg0);
void bursterAwakeTick(Task* arg0);
void bursterContacts(Task* arg0);
void bursterTakeDamage(Task* arg0, s32 arg1);
void bursterTurnToPlayer(Task* arg0);
void bursterDeathState(Enemy* enemy, Task* task);
void bursterKill(Task* arg0, u8 arg1);
void bursterDropSpawnState(Enemy* arg0, Task* arg1);
void bursterDropState(Enemy* arg0, Task* arg1);
void bursterDropCollide(Task* arg0);
s32  bursterMessage(Task* arg0, s32 arg1, ActorCommand* request);
void bursterUpdateState(Enemy* arg0, Task* arg1);
void bursterReactionFlags(Task* arg0);
void bursterStep(Task* task);
void bursterScalePart(Task* arg0, GfxCoord* arg1);
void bursterFlatten(Task* arg0);
void bursterExit(Task* task);
void bursterFallStep(Task* task);

/* Defined by each package. */
void bursterAnimate(Task* arg0);
void bursterColour(Enemy* arg0, Task* task);
void bursterDrawShadow(Task* task);

#endif /* SRC_SHARED_BURSTER_H */
