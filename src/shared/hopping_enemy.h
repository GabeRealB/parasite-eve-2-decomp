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

void hopperDrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade);
void hopperSpawn(Task* task);
void hopperSpawnHidden(Task* task);
void hopperWalkStart(Task* arg0);
void hopperWalkApproach(Task* arg0);
void hopperLeapAttack(Task* arg0);
void hopperLeapTurnAway(Task* arg0);
void hopperLeapRebound(Task* arg0);
void hopperDangleFrame(Task* arg0);
void hopperDangleFall(Task* arg0);
void hopperDangleLand(Task* arg0);
void hopperApplyContacts(Task* arg0, s16 arg1);
void hopperTickAnim(Task* arg0);
void hopperShrinkWithDust(Task* arg0);
void hopperTrackPlayer(Task* arg0);
void hopperRecoilRecover(Task* arg0);
void hopperLurkLookAround(Task* arg0);
void hopperLurkSidestepToCombat(Task* arg0);
void hopperLurkSidestepRight(Task* arg0);
void hopperLurkSidestepLeft(Task* arg0);
void hopperEmergeAtSpot(Task* arg0);
void hopperEmergeBackflip(Task* arg0);
void hopperEmergeHopForward(Task* arg0);
void hopperEmergeArcBack(Task* arg0);
void hopperEmergeHopBack(Task* arg0);
void hopperEmergeBackOff(Task* arg0);
void hopperEmergeHighArc(Task* arg0);
void hopperEmergeFlipOver(Task* arg0);
void hopperPulledStruggle(Task* arg0);
void hopperPulledIn(Task* arg0);
void hopperPulledLimp(Task* arg0);
void hopperLoadSoundBank(void);
void hopperSetAlertHold(Task* arg0, s32 arg1);
s32  hopperTakeHitRequest(Task* arg0);
void hopperCommandMsg(Task* arg0, s32 arg1, ActorCommand* request);
void hopperTurnToPlayer(Task* arg0, s32 step);
void hopperStatusHold(Task* arg0);
void hopperKnockdownStart(Task* arg0);
void hopperKnockdownRise(Task* arg0);
void hopperKnockdownEnd(Task* arg0);
void hopperWalkFinish(Task* arg0);
void hopperLeapLand(Task* arg0);
void hopperAlertCry(Task* arg0);
void hopperAlertRelease(Task* arg0);
void hopperAlertCrouch(Task* arg0);
void hopperAlertSidestep(Task* arg0);
void hopperDangleSway(Task* arg0);
void hopperDeathCry(Task* arg0);
void hopperDeathSettle(Task* arg0);
void hopperDeathWaitAnim(Task* arg0);
void hopperDeathTurnTranslucent(Task* arg0);
void hopperDespawn(Task* arg0);
void hopperRecoilLight(Task* arg0);
void hopperRecoilHeavy(Task* arg0);
void hopperRecoilHeavyEnd(Task* arg0);
void hopperLurkRiseState(Task* arg0);
void hopperLurkWait(Task* arg0);
void hopperLurkIdleEnd(Task* arg0);
void hopperLurkCrouch(Task* arg0);
void hopperLurkRaise(Task* arg0);
void hopperLurkRiseEnd(Task* arg0);
void hopperLurkBrace(Task* arg0);
void hopperLurkShiftStart(Task* arg0);
void hopperLurkShiftBrace(Task* arg0);
void hopperPullStart(Task* arg0);
void hopperPullReact(Task* arg0);
void hopperVanish(Task* arg0);
void hopperVanishFree(Task* arg0);
void hopperDeathSettleQuiet(Task* arg0);
void hopperDeathCryUnlink(Task* arg0);
void hopperShrink(Task* arg0);
s32  hopperTakeKnockdownRequest(Task* arg0);

/* Defined by each package. */
s32  hopperScaleBySpeed(Task* arg0, s16 arg1);
s16  hopperAnimEnded(Task* arg0);
s16  hopperJoinAlert(Task* arg0);
void hopperLurkRiseStart(Task* arg0);
void hopperDangleState(Task* arg0);

static inline void hopperEnter_state(Task* arg0, s32 state);
static inline void hopperUpdate_color(void* enemy, GfxCoord* coord);
static inline void hopperCalc_push(Task* arg0, GfxCoord* coord, WorldCollisionContact* rec, SVECTOR* out);
static inline void hopperSet_state(Task* arg0, s32 state);
static inline s32  hopperTake_request(Task* arg0);
static inline s32  hopperIs_hit(Task* arg0);
static inline void hopperSet_state_s16(Task* arg0, s16 state);

#endif /* SRC_SHARED_HOPPING_ENEMY_H */
