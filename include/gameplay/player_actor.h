#ifndef GAMEPLAY_PLAYER_ACTOR_H
#define GAMEPLAY_PLAYER_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

struct GfxCoord;

/// Scratch-stack block for the turn a scripted walk makes toward `GameActor.destination`.
///
/// Holds the destination's displacement from the model's root coordinate,
/// whose X and Z give the heading to walk along, and the part of the turn
/// toward that heading the actor makes this frame. The player's walk-to-point
/// state and a companion package's variants of it each reserve one block per
/// call and release it before returning. Angles use 4096 units per turn.
typedef struct {
    s32     turnStep;    // Shortest signed turn onto the heading of `targetDelta`, then clamped to the state's per-frame rate
    VECTOR3 targetDelta; // `GameActor.destination` minus the root coordinate's translation
    byte    field_10[4]; // Never accessed; role unproven
} PlayerActorApproachScratch;
STATIC_ASSERT_SIZEOF(PlayerActorApproachScratch, 0x14);

/// Scratch-stack block for marking where a weapon's attack struck the room.
///
/// The search walks the attacking actor's weapon contacts for the room
/// geometry hit nearest the weapon, skipping surfaces that show no weapon
/// impacts, and spawns the impact effect on a temporary coordinate node placed
/// there. The player's routine, which the weapon packages call, and the armed
/// companion package's own copy each reserve one block per call and release it
/// before returning. The block is not cleared.
///
/// Positions are those of the contact points: the space the weapon node's
/// composed transform is expressed in. Only the translation of the impact
/// node's composed transform is written; its rotation, which the effect
/// spawner applies to `jitter`, is whatever the scratch stack last held.
/// The spawned effects are also handed the addresses of `impactCoord` and
/// `jitter` and keep them past the block's release; whether any effect reads
/// through them afterwards is unproven.
typedef struct {
    WorldCollisionDelta pushback;    // Correction the contact resolver writes for the hit being tested; never read, the resolver is run for the surface it reports
    GfxCoord            impactCoord; // Parentless node with a supplied composed transform whose translation is the chosen contact point
    SVECTOR             jitter;      // Random 0..7 per axis: the effect's offset from the node, also added to the position reported to the caller
} PlayerActorWeaponImpactScratch;
STATIC_ASSERT_SIZEOF(PlayerActorWeaponImpactScratch, 0x68);

/// 2-wide rows indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId`. `Gp_PlayerMode2StateB` passes
/// `D_80112E04[field_22][1]` to `func_80105894`.
extern u8 D_80112E04[][2];

/// u16 table indexed by `Gp_AttachActorObj` arg1: the reach a weapon of that
/// attach id adds to the shape's `ends[1]` to give its `ends[0]`.
extern u16 D_80112F60[];

extern u16 Gp_WeaponIdBase[2];

extern AnimationBank* Gp_PlayerAnimBlkTbl[34];

/// Queues sound event `sfx` from the object's world position, panned and
/// depth-attenuated by `worldCoordGetOriginAudioPan` / `worldCoordGetOriginAudioDepth`. A third argument of
/// 1 raises the mid-action bit alongside it; the role of that argument at the
/// call sites is not established.
void Gp_PlayObjSfx(GfxCoord* coord, s32 sfx, s32 arg2);

void Gp_PulseState1C80(void);

void func_800FDB18(s32 arg0, struct GfxCoord* arg1, SVECTOR* arg2, EffectSpawnArg* arg3);

s32 func_801011D0(struct GfxCoord* arg0, WorldCollisionContact* arg1, s32 arg2, s32* arg3);

void Gp_AttachActorObj(Task* arg0, s32 arg1, s32 arg2);

/// Starts a selected animation set on the player or companion actor's child slots with a pose blend.
///
/// `task->work` must be a live `GameActor` with initialized playback bound to
/// its slots and word-aligned pose buffer. Visits slots 1 through
/// `animationSlotCount - 1`; a count at most 1 performs no playback. The active
/// prefix must fit the actor's storage, and each slot's existing track and
/// coordinate indices must fit the selected clip and model.
///
/// Each slot first ticks at its existing rate to apply its pose to the model
/// and capture an encoded transition pose; skipped writes retain the buffer.
/// It then selects the track start from the actor's current `animationSets`
/// table, following control records, and takes the actor's `animationRate`
/// for subsequent ticks. Pose encoding
/// and the capture tick's flags and boundary latch are retained.
///
/// The low 16 bits of `setIndex` must select a loaded set, excluding
/// `ANIMATION_SET_BUFFERED_POSE`. `blendFrames` counts whole normal-rate frames
/// (0..2047); zero gives no transition time but still captures the prior pose.
/// `unusedArgument` is ignored and retained for the calling convention.
/// No bounds are checked. Keep the borrowed set table, clip data, model and
/// actor storage live during playback; record bounds, terminating control
/// walks, supported encodings, scratch capacity and GTE requirements are those
/// of `animationTickSlotPose`.
void playerActorPlayChildSlotsWithBlend(Task* task, s32 setIndex, s32 unusedArgument, s32 blendFrames);

Task* func_80104258(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

Task* Gp_SpawnWeaponEff(void);

void func_80106350(Task* arg0, s32 arg1, s32 arg2);

/// Message 1006; the fourth dispatch argument is unused.
s32 func_80104E00(Task* arg0, s32 arg1, ActorTransform* transform, s32 unusedArg3);

s32 Gp_PickNearestRec18(WorldCollisionContact* arg0, struct GfxCoord* arg1, struct GfxCoord* arg2);

s32 func_80105894(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_80106238(Task* arg0, s32 arg1, s32 arg2);

void Gp_AnimResetChildSlots(Task* arg0, s32 arg1);

void func_80106550(Task* arg0);

void Gp_AnimTickChildSlots(Task* arg0);

s32 func_80106264(s32 arg0);

void func_801066DC(Task* arg0, s16 arg1);

s16 func_80103E7C(s16 arg0, s16 arg1);

void Gp_StepPlayerMove(Task* arg0);

s32 Gp_KillPlayerEffs(void);

void Gp_TurnPlayer(Task* arg0);

s32 func_801060E0(Task* arg0);

void func_80103C74(GfxCoord* arg0, VECTOR3* arg1, VECTOR3* arg2);

s32 func_80103D8C(s32 arg0, s32 arg1);

void Gp_AnimPlayChildSlots(Task* arg0, s32 arg1, s32 arg2);

void Gp_TickActorAnimState(Task* arg0);

void Gp_TrackLockTarget(Task* arg0);

Task* func_80104490(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_80106518(s32 arg0);

Task* func_80104364(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_801041B4(Task* arg0);

void Gp_PlayerMode2State4(Task* arg0);

/// Message 1009; the fourth dispatch argument is unused.
s32 Gp_EnterActorMode2(Task* arg0, s32 arg1, s32 arg2, s32 unusedArg3);

void func_80105B74(VECTOR3* arg0);

void Gp_PlayerWorkTask(Task* arg0);

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1);

void Gp_PlaceCoordOffset(GfxCoord* arg0, GfxCoord* arg1, SVECTOR* arg2);

s32 func_80105ED4(Task* arg0);

void Gp_PlayerMode2State0(Task* arg0);

void Gp_PlayerMode2State1(Task* arg0);

void Gp_PlayerMode2State2(Task* arg0);

void Gp_PlayerMode2State6(Task* arg0);

s32 func_80104684(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg);
s32 func_80104D68(Task* arg0, s32 arg1, ActorTransform* transform, s32 unusedSecondArg);
s32 func_801052B8(Task* arg0, s32 arg1, GameActorWalkSteps* walkSteps, s32 unusedSecondArg);
s32 func_80105828(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
s32 func_8010583C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32 func_801058BC(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg);
s32 func_80105A60(Task* arg0, s32 arg1, GfxCoord* arg2, s32 unusedSecondArg);
s32 func_80105AB0(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg);

#endif // GAMEPLAY_PLAYER_ACTOR_H
