/* The boss fought in the Shelter B3 dumping hole / garbage incinerator (area
 * 0x27). A large jointed host with seven escort models charges along the
 * arena's x axis, turns and charges back, pushing a collision wall rebuilt in
 * front of it. It yaws and pitches its neck parts toward the player and swings
 * a seven-segment limb whose extension is a shared reach counter. It cross-
 * fades three animation pairs. It flings sub-enemies: a chunk thrown forward
 * from escort 0 that falls with a ground shadow; a glob spat from escort 1
 * that bounces, stretches and, if the player is in reach, engulfs them by
 * installing a caught animation on the player; debris chunks from the owner's
 * part 3 that drop, slide and settle with smoke puffs; blobs launched high
 * off-screen that rain onto points on a ring around the host and splat flat;
 * and spinners that wait hidden, then spiral toward a target point. A shared
 * end flag makes every sub-enemy tear itself down when the fight ends. It uses
 * ActorContact_TurnJoint and ActorContact_PushContact from the existing
 * actor_contacts library.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_INCINERATOR_BOSS_H
#define SRC_SHARED_INCINERATOR_BOSS_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

void incinBossBuildWall(Task* task, s16 scale, s16 drop, s16 index);
void incinBossPoseLimb(Task* task);
void incinBossTurnNeck(Task* task, s16 arg1);
void incinBossPitchNeck(Task* task, s16 arg1);
void incinBossSeedBlend(Task* task);
void incinBossSwitchAnim(Task* arg0);
void incinBossTickBlended(Task* arg0);
void incinBossTickAnim(Task* arg0);
void incinBossHitEffect(GfxCoord* coord, s32 id);
void incinBossThrowSpawn(GpEnemy* enemy, Task* task);
void incinBossThrowFly(GpEnemy* enemy, Task* task);
void incinBossGlobSpawn(GpEnemy* enemy, Task* task);
void incinBossGlobFall(GpEnemy* enemy, Task* task);
void incinBossGlobEngulf(GpEnemy* enemy, Task* task);
void incinBossGlobHold(GpEnemy* enemy, Task* task);
void incinBossChunkSpawn(GpEnemy* enemy, Task* task);
void incinBossChunkFall(GpEnemy* enemy, Task* task);
void incinBossChunkSettle(GpEnemy* enemy, Task* task);
void incinBossRainSpawn(GpEnemy* enemy, Task* task);
void incinBossRainRise(GpEnemy* enemy, Task* task);
void incinBossRainFall(GpEnemy* enemy, Task* task);
void incinBossRainSplat(GpEnemy* enemy, Task* task);
void incinBossSpinnerSpawn(GpEnemy* enemy, Task* task);
void incinBossSpinnerChase(GpEnemy* enemy, Task* task);
void incinBossExit(Task* arg0);
void incinBossPropSetup(GpEnemy* enemy, Task* task);
void incinBossPropTick(GpEnemy* enemy, Task* arg1);
void incinBossSpinnerWait(GpEnemy* arg0, Task* arg1);

static inline void incinShrinkRotation(GfxCoord* coord);
static inline void incinScaleRotation(GfxCoord* coord, s16 xz, s32 y);
static inline void incinGapToCamera(GfxCoord* coord, SVECTOR* out);

#endif /* SRC_SHARED_INCINERATOR_BOSS_H */
