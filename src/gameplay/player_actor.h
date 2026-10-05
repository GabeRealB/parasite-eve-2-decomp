#ifndef GAMEPLAY_PRIVATE_PLAYER_ACTOR_H
#define GAMEPLAY_PRIVATE_PLAYER_ACTOR_H

#include "types.h"

#include "actor.h"
#include "gameplay/actor_spawn_types.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/message.h"
#include "gameplay/world_targets_types.h"

#include "main/session_types.h"
#include "main/task_types.h"

/// Scratch-stack reservation for the displacement used by a planar-distance query.
///
/// The complete 16-byte block is borrowed until the query returns. Only XYZ
/// are accessed; the final four bytes have no established role.
typedef struct {
    VECTOR3 delta;      // First point minus second point, in their common coordinate frame
    byte    field_C[4]; // Never accessed; role unproven
} PlayerActorPlanarDistanceScratch;
STATIC_ASSERT_SIZEOF(PlayerActorPlanarDistanceScratch, 0x10);

/// Pending world-collision updates for the player's body and held-object nodes.
enum {
    PLAYER_ACTOR_WORLD_COLLISION_ENABLE  = 7,
    PLAYER_ACTOR_WORLD_COLLISION_DISABLE = 0x38,
};

/// Bank sentinel for a directly installed animation-set table.
enum { PLAYER_ACTOR_DIRECT_ANIMATION_BANK = 0x7FFF };

/// u8 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant`. Non-zero selects
/// `Gp_AimPitchToLock`; zero uses `D_actor_800100_80167218` with `Gp_AimPitchRec`.
extern u8 D_80113388[];

extern TaskDesc D_80113340[2];

extern EffectSpawnArg D_80113358;

extern u16 Gp_AllyIdBase[4];

extern AnimationBank* Gp_AnimBlkTbl[8];

void Gp_UpdatePlayerMove(void);

void Gp_EffSprTask81(Task* arg0);

void func_800F91AC(Task* arg0);

void Gp_EffCtlTask9B(Task* arg0);

void Gp_EffSprTask30(Task* arg0);

void Gp_EffCtlTaskF3(Task* arg0);

void Gp_EffCtlTaskAC(Task* arg0);

/// Runs the Berserker shot-burst controller, gameplay effect bank 6 slot 0x0E.
///
/// Uses the task's owned `EffectWork` spawn argument and coordinate body. First
/// running tick parents an identity transform at the live player's part 8 and
/// enables the PE-status burst flag. Running ticks consume `burstRequest` by
/// drawing the radial burst and two discs. The task persists while Berserker,
/// the PE flag and engaged battle state all hold; otherwise it clears the screen
/// burst guard and releases its work/task. Effect control can pause it or cancel
/// it directly; direct cancellation does not clear that guard.
void effectControlTask0E(Task* task);

void Gp_EffCtlTaskA5(Task* arg0);

void Gp_EffCtlTaskA6(Task* arg0);

void Gp_EffCtlTaskE3(Task* arg0);

void func_800FF710(Task* arg0);

void func_801088D4(Task* arg0, s32 arg1, s32 arg2);

s32 Gp_SetActorDest(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim);

s32 Gp_MoveActorBy(Task* arg0, s32 arg1, GameActorMoveBy* move, s32 unusedSecondArg);

Task* Gp_SpawnPlayer(const ActorSpawnTransform* spawnTransform, u16 arg1, s32 arg2, ActorSpawnOptions* options);

void func_801061F0(void);

/// Installs a borrowed set table and enters scripted player animation playback.
s32 func_80104B54(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);

/// Enters the player's normal-mode aim-entry state and starts its child-slot clip.
///
/// Stops movement and turning, resets phase and attack cooldown and enables the
/// controller that blends into the holding clip once all child slots settle.
/// Zero `blendFrames` restarts clip 7 directly; a nonzero value blends from the
/// captured poses for that many whole normal-rate frames (1..2047). The live
/// actor and native clip table must meet the child-slot playback contracts.
void playerActorEnterAim(Task* task, s32 blendFrames);

void func_80108874(Task* arg0);

void func_800FAA14(Task* arg0);

void Gp_EffCtlTask07(Task* arg0);

void Gp_EffCtlTask7F(Task* arg0);

void Gp_AimYawToLock(Task* arg0, s32 arg1);

void Gp_AimPitchToLock(Task* arg0);

void Gp_AimPitchRec(Task* arg0, s32 arg1, s32 arg2);

void Gp_DetachLinkNode(Task* arg0);

/// Selects an actor's lock target and transfers the targeted mark from its old node.
///
/// Requires live `GameActor` work and a non-NULL target. The target is borrowed
/// and must remain live while selected; a distinct previous target must stay
/// live through this call. Selecting the same node marks it targeted again.
void playerActorSetLockTarget(Task* task, WorldTargetNode* target);

#endif // GAMEPLAY_PRIVATE_PLAYER_ACTOR_H
