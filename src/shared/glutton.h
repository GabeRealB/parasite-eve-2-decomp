/* The Glutton (actor_403200 and actor_444000), fought in the Shelter B3
 * dumping hole / garbage incinerator (area 0x27). A large jointed host with seven escort models charges along the
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

#ifndef SRC_SHARED_GLUTTON_H
#define SRC_SHARED_GLUTTON_H

/// The Glutton's host task, as the helpers reach it. A package whose symbol
/// is a wider object holding the pointer defines this as the member before
/// including the header.
#ifndef GLUTTON_HOST_TASK
#define GLUTTON_HOST_TASK gGluttonHostTask
#endif

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

void gluttonBuildWall(Task* task, s16 scale, s16 drop, s16 index);
void gluttonPoseLimb(Task* task);
void gluttonTurnNeck(Task* task, s16 arg1);
void gluttonPitchNeck(Task* task, s16 arg1);
void gluttonSeedBlend(Task* task);
void gluttonSwitchAnim(Task* arg0);
void gluttonTickBlended(Task* arg0);
void gluttonTickAnim(Task* arg0);
void gluttonHitEffect(GfxCoord* coord, s32 id);
void gluttonThrowSpawn(Enemy* enemy, Task* task);
void gluttonThrowFly(Enemy* enemy, Task* task);
void gluttonGlobSpawn(Enemy* enemy, Task* task);
void gluttonGlobFall(Enemy* enemy, Task* task);
void gluttonGlobEngulf(Enemy* enemy, Task* task);
void gluttonGlobHold(Enemy* enemy, Task* task);
void gluttonChunkSpawn(Enemy* enemy, Task* task);
void gluttonChunkFall(Enemy* enemy, Task* task);
void gluttonChunkSettle(Enemy* enemy, Task* task);
void gluttonRainSpawn(Enemy* enemy, Task* task);
void gluttonRainRise(Enemy* enemy, Task* task);
void gluttonRainFall(Enemy* enemy, Task* task);
void gluttonRainSplat(Enemy* enemy, Task* task);
void gluttonSpinnerSpawn(Enemy* enemy, Task* task);
void gluttonSpinnerChase(Enemy* enemy, Task* task);
void gluttonExit(Task* arg0);
void gluttonPropSetup(Enemy* enemy, Task* task);
void gluttonPropTick(Enemy* enemy, Task* arg1);
void gluttonSpinnerWait(Enemy* arg0, Task* arg1);

static inline void gluttonShrinkRotation(GfxCoord* coord);
static inline void gluttonScaleRotation(GfxCoord* coord, s16 xz, s32 y);
static inline void gluttonGapToCamera(GfxCoord* coord, SVECTOR* out);

void gluttonGlobTask(Task* arg0);
void gluttonChunkTask(Task* arg0);
void gluttonSpinnerTask(Task* arg0);
void gluttonRainTask(Task* arg0);
void gluttonThrowTask(Task* arg0);
void gluttonPropTask(Task* arg0);
void gluttonSetQuadHeights(s32 arg0, s16 arg1);
void gluttonSetShakeLevel(s8 arg0);
void gluttonSetSpinnersReleased(s16 arg0);
s16  gluttonGetSpinnersReleased(void);

#endif /* SRC_SHARED_GLUTTON_H */
