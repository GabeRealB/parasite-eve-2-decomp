/* Code of the Mad Chaser shared by actor_04400 (actor_104400/342200),
 * actor_341700 and actor_342400: it twists its spine by a third of the turn per joint, links
 * its three collision spheres, pins a model part to a world position and
 * starts its death squash. The two later packages also share a creep-forward-
 * until-hit state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The code sees the work block as Actor341700Work. The
 * later two packages name the creep state twice in their state table; the
 * second entry includes the fragment again under its own name.
 */

#ifndef SRC_SHARED_MAD_CHASER_H
#define SRC_SHARED_MAD_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

void madChaserTwistSpine(Task* arg0);
void madChaserLinkBodies(Task* arg0);
void madChaserPinPart(Task* arg0, s16 part, SVECTOR3* pos);
void madChaserBeginDeath(Task* arg0);
void madChaserCreepUntilHit(Task* arg0);

void madChaserStartLeap(Task* arg0);
void madChaserStartHold(Task* arg0);
void madChaserStartAlert(Task* arg0);
void madChaserBurst(Task* arg0);
void madChaserDropBodies(Task* arg0);
void madChaserBeginShrink(Task* task);

void madChaserSpawnGibs(Task* arg0);

void madChaserDrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade);
void madChaserSpawn(Task* task);
void madChaserSpawnHidden(Task* task);
void madChaserWalkStart(Task* arg0);
void madChaserWalkApproach(Task* arg0);
void madChaserLeapAttack(Task* arg0);
void madChaserLeapTurnAway(Task* arg0);
void madChaserLeapRebound(Task* arg0);
void madChaserDangleFrame(Task* arg0);
void madChaserDangleFall(Task* arg0);
void madChaserDangleLand(Task* arg0);
void madChaserApplyContacts(Task* arg0, s16 arg1);
void madChaserTickAnim(Task* arg0);
void madChaserShrinkWithDust(Task* arg0);
void madChaserTrackPlayer(Task* arg0);
void madChaserRecoilRecover(Task* arg0);
void madChaserLurkLookAround(Task* arg0);
void madChaserLurkSidestepToCombat(Task* arg0);
void madChaserLurkSidestepRight(Task* arg0);
void madChaserLurkSidestepLeft(Task* arg0);
void madChaserEmergeAtSpot(Task* arg0);
void madChaserEmergeBackflip(Task* arg0);
void madChaserEmergeHopForward(Task* arg0);
void madChaserEmergeArcBack(Task* arg0);
void madChaserEmergeHopBack(Task* arg0);
void madChaserEmergeBackOff(Task* arg0);
void madChaserEmergeHighArc(Task* arg0);
void madChaserEmergeFlipOver(Task* arg0);
void madChaserPulledStruggle(Task* arg0);
void madChaserPulledIn(Task* arg0);
void madChaserPulledLimp(Task* arg0);
void madChaserLoadSoundBank(void);
void madChaserSetAlertHold(Task* arg0, s32 arg1);
s32  madChaserTakeHitRequest(Task* arg0);
void madChaserCommandMsg(Task* arg0, s32 arg1, ActorCommand* request);
void madChaserTurnToPlayer(Task* arg0, s32 step);
void madChaserStatusHold(Task* arg0);
void madChaserKnockdownStart(Task* arg0);
void madChaserKnockdownRise(Task* arg0);
void madChaserKnockdownEnd(Task* arg0);
void madChaserWalkFinish(Task* arg0);
void madChaserLeapLand(Task* arg0);
void madChaserAlertCry(Task* arg0);
void madChaserAlertRelease(Task* arg0);
void madChaserAlertCrouch(Task* arg0);
void madChaserAlertSidestep(Task* arg0);
void madChaserDangleSway(Task* arg0);
void madChaserDeathCry(Task* arg0);
void madChaserDeathSettle(Task* arg0);
void madChaserDeathWaitAnim(Task* arg0);
void madChaserDeathTurnTranslucent(Task* arg0);
void madChaserDespawn(Task* arg0);
void madChaserRecoilLight(Task* arg0);
void madChaserRecoilHeavy(Task* arg0);
void madChaserRecoilHeavyEnd(Task* arg0);
void madChaserLurkRiseState(Task* arg0);
void madChaserLurkWait(Task* arg0);
void madChaserLurkIdleEnd(Task* arg0);
void madChaserLurkCrouch(Task* arg0);
void madChaserLurkRaise(Task* arg0);
void madChaserLurkRiseEnd(Task* arg0);
void madChaserLurkBrace(Task* arg0);
void madChaserLurkShiftStart(Task* arg0);
void madChaserLurkShiftBrace(Task* arg0);
void madChaserPullStart(Task* arg0);
void madChaserPullReact(Task* arg0);
void madChaserVanish(Task* arg0);
void madChaserVanishFree(Task* arg0);
void madChaserDeathSettleQuiet(Task* arg0);
void madChaserDeathCryUnlink(Task* arg0);
void madChaserShrink(Task* arg0);
s32  madChaserTakeKnockdownRequest(Task* arg0);

/* Defined by each package. */
s32  madChaserScaleBySpeed(Task* arg0, s16 arg1);
s16  madChaserAnimEnded(Task* arg0);
s16  madChaserJoinAlert(Task* arg0);
void madChaserLurkRiseStart(Task* arg0);
void madChaserDangleState(Task* arg0);

static inline void madChaserEnterState(Task* arg0, s32 state);
static inline void madChaserUpdateColor(void* enemy, GfxCoord* coord);
static inline void madChaserCalcPush(Task* arg0, GfxCoord* coord, WorldCollisionContact* rec, SVECTOR* out);
static inline void madChaserSetState(Task* arg0, s32 state);
static inline s32  madChaserTakeRequest(Task* arg0);
static inline s32  madChaserIsHit(Task* arg0);
static inline void madChaserSetStateS16(Task* arg0, s16 state);

void madChaserLurkTick(Task* arg0);
void madChaserCombatTick(Task* arg0);
void madChaserEmergeTick(Task* arg0);
void madChaserShrinkDeathTick(Task* arg0);
void madChaserDropDeathTick(Task* arg0);
void madChaserDeathTick(Task* arg0);
void madChaserLurkAlertState(Task* arg0);
void madChaserWalkState(Task* arg0);
void madChaserHiddenTask(Task* arg0);
void madChaserKnockdownState(Task* arg0);
void madChaserPullState(Task* arg0);
void madChaserTask(Task* arg0);
void madChaserLeapState(Task* arg0);
void madChaserDangleState(Task* arg0);
void madChaserVanishState(Task* arg0);
void madChaserRecoilLightState(Task* arg0);
s16  madChaserJoinAlert(Task* arg0);
void madChaserDeathPause(Task* arg0);
s16  madChaserAnimEnded(Task* arg0);
void madChaserAlertWait(Task* arg0);
void madChaserDangleStart(Task* arg0);
void madChaserMsgPlace(Task* task, s16 part, VECTOR3* pos);
void madChaserStatusHoldStart(Task* arg0);
void madChaserLurkRiseStart(Task* arg0);
s32  madChaserScaleBySpeed(Task* arg0, s16 arg1);
void madChaserAdvanceState(Task* arg0);
void madChaserStartDespawn(Task* arg0);
void madChaserToAlertState(Task* arg0);

#endif /* SRC_SHARED_MAD_CHASER_H */
