/* The behaviour the Rook GOLEM (Beam Sword, actor_102300), the Pawn GOLEM
 * (Grenade Launcher, actor_105600) and the Rook GOLEM (Grenade Launcher,
 * actor_105700) share; the packages differ in sound bank and some attack
 * states. It covers the idle and approach states with their player-proximity
 * check, the knocked-down and recoil states, the dead state that files the
 * enemy's pose, and three per-frame helpers: turning the root toward a target
 * yaw, easing out the hit tilt, and playing an animation's voice cues. Each
 * package defines its own voice-cue table and per-animation blend lengths
 * under the shared names.
 *
 * Each package states its type and weapon before including this header:
 * GOLEM_PAWN_ROOK_TYPE is GOLEM_PAWN or GOLEM_ROOK, GOLEM_PAWN_ROOK_WEAPON is
 * GOLEM_BEAM_SWORD (actor_02000, actor_02300) or GOLEM_GRENADE_LAUNCHER
 * (actor_05600, actor_05700); the parameters below follow from them.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines its tables at its own positions under these
 * names:
 *
 *   s16 gGolemPawnRookAnimBlendFrames[32]  blend-in length per animation, in frames
 *   s32 gGolemPawnRookVoiceCues[17]        the voice-cue sound ids of its sound bank
 */

#ifndef SRC_SHARED_GOLEM_PAWN_ROOK_H
#define SRC_SHARED_GOLEM_PAWN_ROOK_H

#define GOLEM_PAWN             1
#define GOLEM_ROOK             2
#define GOLEM_BEAM_SWORD       1
#define GOLEM_GRENADE_LAUNCHER 2

/* Per weapon: the lunge cycle's wind-up turn rate and lunge range, and the
 * state and animation it hands over to for the lunge and for the walk-in (the
 * Grenade Launcher's state table is the longer one). The Beam Sword's charge
 * turns at GOLEM_PAWN_ROOK_CHARGE_TURN, which only the Pawn does. */
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_BEAM_SWORD
#define GOLEM_PAWN_ROOK_WIND_UP_TURN 0x3C
#define GOLEM_PAWN_ROOK_LUNGE_RANGE  0x8CA
#define GOLEM_PAWN_ROOK_LUNGE_STATE  4
#define GOLEM_PAWN_ROOK_LUNGE_ANIM   8
#define GOLEM_PAWN_ROOK_WALK_STATE   3
#define GOLEM_PAWN_ROOK_WALK_ANIM    5
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN
#define GOLEM_PAWN_ROOK_CHARGE_TURN 0xF
#else
#define GOLEM_PAWN_ROOK_CHARGE_TURN 0
#endif
#else
#define GOLEM_PAWN_ROOK_WIND_UP_TURN 0x1E
#define GOLEM_PAWN_ROOK_LUNGE_RANGE  0x7D0
#define GOLEM_PAWN_ROOK_LUNGE_STATE  7
#define GOLEM_PAWN_ROOK_LUNGE_ANIM   0x10
#define GOLEM_PAWN_ROOK_WALK_STATE   6
#define GOLEM_PAWN_ROOK_WALK_ANIM    0xC
#endif

/* Per build: the type id (also in its collision keys, 0x30000 | id); and for
 * the Grenade Launcher's approach cycle, how long it backs away and how many
 * strikes it counts before the follow-up. */
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_GRENADE_LAUNCHER
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN
#define GOLEM_PAWN_ROOK_ID             0x38
#define GOLEM_PAWN_ROOK_BACKOFF_FRAMES 0x1E
#define GOLEM_PAWN_ROOK_STRIKE_LIMIT   6
#else
#define GOLEM_PAWN_ROOK_ID             0x39
#define GOLEM_PAWN_ROOK_BACKOFF_FRAMES 0x3C
#define GOLEM_PAWN_ROOK_STRIKE_LIMIT   3
#endif
#endif

/// The impact cue's sound id, as the bullet reads it. A package whose symbol
/// is a wider object holding the id defines this as the member before
/// including the header.
#ifndef GOLEM_PAWN_ROOK_IMPACT_SOUND
#define GOLEM_PAWN_ROOK_IMPACT_SOUND gGolemPawnRookImpactSound
#endif

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

/// Work block of that enemy, allocated at its full size and kept at
/// `Task::work`: the animation context with its nineteen slots and pose
/// records, the light and colour matrices, the body objects with their
/// collision records, and the animation and state halfwords the handlers
/// drive.
typedef struct GolemPawnRookWork {
    ActorAnimRig19 rig;
    MATRIX         field_43C;
    MATRIX         field_45C;
    /// First body object, handed to `Gp_UnlinkObj` by the teardown of
    /// `Actor05700_Fn01A58`; its `pos.vz` is the pose the state-0
    /// branch of `Actor05700_Fn01318` parks (-0xA7 or 0x109) and its
    /// `radius` the frame count parked alongside it.
    WorldCollisionBody    field_47C;
    WorldCollisionCapsule field_49C;
    WorldCollisionContact field_4B4[1];
    /// Second body object; `pos.vz` is the pose the state-0 branch parks
    /// (0x15E) and `flags` the bits whose 0x4000 it raises.
    WorldCollisionBody    field_4CC;
    WorldCollisionContact field_4EC[5];
    /// Third body object; `flags` is the field whose bit 0x4000 the state-0
    /// branch clears.
    WorldCollisionBody    field_564;
    WorldCollisionContact field_584[4];
    /// Fourth body object: `key` is the object `Gp_PackPair` hands it when
    /// `field_698` first reaches the animation's 0x1C mark and `flags` the
    /// bits whose 0x8000 is raised with it and dropped at the 0x28 mark
    /// (`Actor05700_Fn023AC`).
    WorldCollisionBody    field_5E4;
    WorldCollisionContact field_604[1];
    /// Fifth body object, unlinked with the others by `Actor05700_Fn01A58`.
    WorldCollisionBody    field_61C;
    WorldCollisionCapsule field_63C;
    WorldCollisionContact field_654[1];
    TaskDesc*             field_66C;
    EffectSpawnArg        field_670;
    s32                   field_678;
    s32                   field_67C;
    s32                   field_680;
    byte                  pad_684[4];
    /// Tilt angles decayed toward zero by `Actor05700_Fn016D0`.
    SVECTOR     field_688;
    EffectWork* field_690;
    /// Animation index selected by the state machine; 4 is the "handover"
    /// clip of `Actor05700_Fn04CC0`'s state 0.
    s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    s16 field_696;
    s16 field_698; ///< current frame of the playing clip
    s16 field_69A;
    s16 field_69C; ///< dwell counter, cleared on state 0 entry
    s16 field_69E; ///< dwell counter, cleared on state 0 entry
    u16 field_6A0; ///< sound flags; bit 5/4 gate the two cues
                   /// Current yaw, walked toward `field_6A4` by
                   /// `Actor05700_Fn01544`, using `field_69E` as the per-frame step.
    s16 field_6A2;
    s16 field_6A4; ///< yaw the actor wants to face
    s16 field_6A6; ///< parked animation for the state-F0 path
    s16 field_6A8; ///< state-machine step
    s16 field_6AA; ///< animation the state-0 branch picks
    s16 field_6AC;
    s16 field_6AE; ///< state-0 frame budget
    s16 field_6B0;
    s16 field_6B2; ///< non-zero forces the state-F0 path
    s16 field_6B4; ///< cleared once the tilt has settled
    s16 field_6B6;
    /// State-0 branch selector: 1 picks the short dwell and animation 1,
    /// 2 the long dwell and animation 2.
    s16  field_6B8;
    s16  field_6BA;
    s16  field_6BC;
    s16  field_6BE;
    s16  field_6C0;
    s16  field_6C2;
    s16  field_6C4;
    byte pad_6C6[4];
    /// Body variant select: `Actor05700_Fn01A58` drops the fifth body
    /// object for the two values 0x38 / 0x39 and hands the halfword to
    /// `Gp_ReleaseStateF0Add`.
    s16 field_6CA;
    s16 field_6CC;
    s16 field_6CE;
    s16 field_6D0;
    /// Spawn state driven by `Actor05700_Fn05310`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    s16 field_6D2;
    /// Latched on state-0 entry, cleared when the frame budget runs out.
    s16 field_6D4;
    s16 field_6D6; ///< animation index, used as a table row
    s16 field_6D8;
    s16 field_6DA; ///< state-0 frame budget, drained by `field_69C`
    s16 field_6DC;
    /// State-1 step gate: 1 while the state-0 exit is still to be seen, 2
    /// once it has been.
    s16 field_6DE;
    /// State-1 branch selector: zero picks the short dwell and animation 2,
    /// non-zero the long dwell and animation 0x14.
    s16  field_6E0;
    byte pad_6E2[2];
} GolemPawnRookWork;
STATIC_ASSERT_SIZEOF(GolemPawnRookWork, 0x6E4);

/// 0xF0-byte body block `Actor05600_Fn031B0` parks at `Task::work`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `WorldCollisionBody` bodies collide against `rec60`
/// (shared by the first two) and, through the `WorldCollisionCapsule` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct GolemPawnRookFxWork {
    MATRIX                colorMtx;
    MATRIX                lightMtx;
    WorldCollisionBody    obj40;
    WorldCollisionContact rec60[1];
    WorldCollisionBody    obj78;
    WorldCollisionBody    obj98;
    WorldCollisionCapsule d4rec;
    WorldCollisionContact recD0[1];
    s16                   field_E8;
    s16                   field_EA;
    /// Teardown step: 0 unlinks the three bodies, 1 counts `field_E8` up to
    /// the frame the task is destroyed on.
    s16 field_EC;
    s16 field_EE;
} GolemPawnRookFxWork;
STATIC_ASSERT_SIZEOF(GolemPawnRookFxWork, 0xF0);

/// 0x40-byte scratch carved off the scratch stack by `Actor05600_Fn02548`:
/// the converted matrix, the `gte_rtv0` output and the two vectors fed through
/// it (`rot` and `vec` are also the pair handed to `Actor05600_Fn02950`).
typedef struct GolemPawnRookAimScratch {
    MATRIX  mtx;
    VECTOR  pos;
    SVECTOR rot;
    SVECTOR vec;
} GolemPawnRookAimScratch;
STATIC_ASSERT_SIZEOF(GolemPawnRookAimScratch, 0x40);

/// 0x48-byte scratch carved off the scratch stack by `Actor05600_Fn02950`:
/// the beam is walked in eight steps from `vec` to `rot`, each step projected
/// into `cur` (packed screen xy) and `curZ` (OTZ). `xs`/`ys` hold the two
/// projected ends followed by the four offset corners the ribbon polygons are
/// cut from.
typedef struct GolemPawnRookBeamScratch {
    VECTOR  vec;
    SVECTOR pt;
    SVECTOR step;
    s32     prev;
    s32     cur;
    s32     prevZ;
    s32     curZ;
    s16     xs[6];
    s16     ys[6];
} GolemPawnRookBeamScratch;
STATIC_ASSERT_SIZEOF(GolemPawnRookBeamScratch, 0x48);

/// 0x38-byte scratch carved off the scratch stack by
/// `Actor05600_Fn031B0`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct GolemPawnRookPlaceScratch {
    SVECTOR rot;
    VECTOR  pos;
    MATRIX  mtx;
} GolemPawnRookPlaceScratch;
STATIC_ASSERT_SIZEOF(GolemPawnRookPlaceScratch, 0x38);

/// Scratch-pad block of that enemy's push-back and hit: the deltas the
/// collision walk resolves, their normal, the push, and the effect offset and
/// target.
typedef struct GolemPawnRookHitScratch {
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         push;
    SVECTOR        effOfs;
    SVECTOR        target;
} GolemPawnRookHitScratch;
STATIC_ASSERT_SIZEOF(GolemPawnRookHitScratch, 0x40);

void golemPawnRookIdleState(Task* arg0);
void golemPawnRookApproachState(Task* arg0);
void golemPawnRookCheckProximity(Task* arg0);
void golemPawnRookRecoilState(Task* arg0);
void golemPawnRookCollapseState(Task* arg0);
void golemPawnRookDownedShiftState(Task* arg0);
void golemPawnRookDownedFinishState(Task* arg0);
void golemPawnRookTurnTowardTarget(Task* arg0);
void golemPawnRookDecayHitTilt(Task* arg0);
void golemPawnRookPlayAnimCues(Task* arg0);
void golemPawnRookDeadState(Enemy* arg0, Task* arg1);

/* Implemented by each package's hit and push handler. */
void golemPawnRookTakeHits(Task* arg0);
void golemPawnRookKnockdownState(Task* arg0);
void golemPawnRookChargeState(Task* arg0);
void golemPawnRookLungeCycle(Task* arg0);
void golemPawnRookCompanionCycle(Task* arg0);
void golemPawnRookSpawn(Enemy* ctx, Task* actor);
void golemPawnRookLungeStrikeState(Task* arg0);
void golemPawnRookAimLaserSight(Task* arg0);
void golemPawnRookDrawLaserBeam(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);
void golemPawnRookBulletSpawn(Enemy* arg0, Task* arg1);
void golemPawnRookBulletFly(Enemy* arg0, Task* arg1);
void golemPawnRookGunTick(Enemy* enemy, Task* task);
void golemPawnRookBulletDestroy(Enemy* arg0, Task* arg1);

void golemPawnRookSilenceScreamState(Task* arg0);
void golemPawnRookFrameState(Enemy* ctx, Task* actor);
void golemPawnRookBurstPartTick(Enemy* arg0, Task* arg1);

static inline void golemPawnRookSpawnDust(Task* actor);
static inline void golemPawnRookApplyReaction(Task* actor);
static inline void golemPawnRookStepRoot(Task* actor);
static inline void golemPawnRookTickAnim(Task* actor);
static inline void golemPawnRookDraw(Task* actor, GfxCoord* coord);

void golemPawnRookFrameStateNoDust(Enemy* ctx, Task* actor);
void golemPawnRookBeamSwingState(Task* arg0);
void golemPawnRookHitReactionState(Task* task);
void golemPawnRookFlagWaitState(Task* task);
void golemPawnRookNopState(Task* task);
void golemPawnRookDelayedEffectSpawn(Enemy* arg0, Task* task);
void golemPawnRookDelayedEffectTick(Enemy* arg0, Task* task);
void golemPawnRookGunSpawn(Enemy* arg0, Task* task);
void golemPawnRookBurstPartSpawn(Enemy* arg0, Task* task);

#endif /* SRC_SHARED_GOLEM_PAWN_ROOK_H */
