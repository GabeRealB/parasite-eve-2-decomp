/* Code of the Mad Chaser shared by actor_04400 (actor_104400/342200),
 * actor_341700 and actor_342400: it twists its spine by a third of the turn per joint, links
 * its three collision spheres, pins a model part to a world position and
 * starts its death squash. The two later packages also share a creep-forward-
 * until-hit state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The code sees the work block as MadChaserWork. The
 * later two packages name the creep state twice in their state table; the
 * second entry includes the fragment again under its own name.
 */

#ifndef SRC_SHARED_MAD_CHASER_H
#define SRC_SHARED_MAD_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Status flags of `MadChaserWork`, read through two widths: every guard
/// tests bit 0 as a halfword and then bits 0x102 as a word.
typedef union MadChaserSlotFlags {
    u32 word;
    u16 half;
} MadChaserSlotFlags;
STATIC_ASSERT_SIZEOF(MadChaserSlotFlags, 0x4);

/// Work block of the enemy whose code both actor_341700 and actor_342400
/// carry. Each allocates it zeroed at its full size and keeps it at
/// `Task::work`. `field_420` / `field_422` are the state and sub-state indices
/// the handler tables walk and `field_412` the per-state frame counter;
/// `field_414` .. `field_41C` are the animation request.
typedef struct MadChaserWork {
    MATRIX           savedRootMtx; // root matrix saved at death, rescaled each frame while the model shrinks
    MATRIX           colorMtx;     // the model's `TmdObject::colorMtx`
    MATRIX           lightMtx;     // the model's `TmdObject::lightMtx`
    VECTOR           field_60;     // position the root snaps back to when blocked
    SVECTOR          field_70;     // origin of slot 4 entry 0's coords[3], carried into view space
    s16              field_78;     // pitch, fed to RotMatrixX
    s16              field_7A;     // heading fed to rsin / rcos
    s16              field_7C;     // roll, fed to RotMatrixZ
    byte             pad_7E[0x2];
    u16              field_80;     // spawn position: root coord.t[0]
    u16              field_82;     // root coord.t[1], after lifting it by 0x3C
    u16              field_84;     // root coord.t[2]
    byte             pad_86[0x2];
    s16              field_88;     // x of the offset to the nearer player actor
    s16              field_8A;     // y of that offset
    s16              field_8C;     // z of that offset
    byte             pad_8E[0x2];
    u16              field_90;     // root coord.t[0], snapshotted with the view-space origin
    u16              field_92;     // root coord.t[1]
    u16              field_94;     // root coord.t[2]
    byte             pad_96[0x2];
    SVECTOR          field_98;     // translation of coords[6] relative to the view
    AnimationContext anim;
    /// First of the nine `AnimationSlot`s handed to `animationInitContext`; the second
    /// overlaps `flags_EC`, so only the first is spelled out.
    AnimationSlot         slot_B4;
    byte                  pad_DC[0x10];
    MadChaserSlotFlags    flags_EC;
    byte                  pad_F0[0x12C];
    byte                  field_21C[0x90]; // `animationInitContext`'s poseBuffer buffer
    WorldCollisionBody    obj_2AC;
    WorldCollisionBody    obj_2CC;
    WorldCollisionContact rec_2EC[8];
    WorldCollisionBody    obj_3AC;
    WorldCollisionContact rec_3CC[2]; // records of `obj_3AC`
    EffectSpawnArg        eff_3FC;    // `func_800FDB18`'s argument record; `coord` is the model's `coords[1]`
    byte                  pad_404[0x8];
    s16                   field_40C;  // heading the root is moved along
    s16                   field_40E;  // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    s16                   field_410;
    u16                   field_412;  // per-state frame counter
    s16                   field_414;  // animation request kind
    s16                   field_416;  // animation id last applied to the slots
    s16                   field_418;  // animation id
    u16                   field_41A;  // frames since the animation was applied
    s16                   field_41C;  // animation speed / step scale
    s16                   field_41E;  // 1 lets `field_448` jump the state machine
    u16                   field_420;  // state index
    u16                   field_422;  // sub-state index
    s16                   field_424;  // yaw added to model parts 3..5, a third each; eased toward zero each frame
    s16                   field_426;
    s16                   field_428;
    s16                   field_42A;
    s16                   field_42C; // frames spent turning toward field_444; 16 enters state 3
    byte                  pad_42E[0x2];
    u16                   field_430; // Y scale while the model shrinks after death
    s16                   field_432; // 1 re-derives the spawn position
    s16                   field_434; // pitch latched when a sway ends, then eased back to zero
    s16                   field_436; // turn step applied to the heading
    s16                   field_438;
    s16                   field_43A; // distance to the nearer player actor
    byte                  pad_43C[0x2];
    s16                   field_43E; // counted down each frame while blocked
    s16                   field_440; // picks animation 5 (zero) or 6 after animation 8
    s16                   field_442; // frame phase driving the pitch sway
    u16                   field_444; // heading to the nearer player actor relative to field_7A, masked to 0xFFF
    s16                   field_446; // randomised hold in frames
    s16                   field_448; // pending state request; 4 moves the task to state 4 once the enemy is dead
    s16                   field_44A;
    u16                   field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    u8                    field_44E; // set while the enemy carries status flag 4/8
    u8                    field_44F; // 1 runs the post-sub-state step
    byte                  pad_450[0x1];
    u8                    field_451;
    byte                  pad_452[0x2];
} MadChaserWork;
STATIC_ASSERT_SIZEOF(MadChaserWork, 0x454);

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
void madChaserCommandMsg(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3);
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
void madChaserMsgPlace(Task* task, s32 part, VECTOR3* pos, s32 arg3);
void madChaserStatusHoldStart(Task* arg0);
void madChaserLurkRiseStart(Task* arg0);
s32  madChaserScaleBySpeed(Task* arg0, s16 arg1);
void madChaserAdvanceState(Task* arg0);
void madChaserStartDespawn(Task* arg0);
void madChaserToAlertState(Task* arg0);

static __inline__ s16  madChaserTakeHit(Task* arg0);
static __inline__ void madChaserUpdateRotation(Task* arg0);
static __inline__ s16  madChaserTakeHitNibble3(Task* arg0);

#endif /* SRC_SHARED_MAD_CHASER_H */
