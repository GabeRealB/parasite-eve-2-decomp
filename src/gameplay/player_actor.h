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
/// `playerActorAimPitchToLock`; zero uses `D_actor_800100_80167218` with `playerActorAimPart6PitchToLock`.
extern u8 D_80113388[];

extern TaskDesc D_80113340[2];

extern EffectSpawnArg D_80113358;

extern u16 Gp_AllyIdBase[4];

extern AnimationBank* Gp_AnimBlkTbl[8];

/// Advances the live player's input/state and prepares its bodies for collision.
///
/// Requires an occupied player task slot with live GameActor/model resources.
/// The state-update hold suppresses only the ordinary state tick; input capture,
/// pending displacement, contact clearing and collision-heading publication
/// still run. Adds 128 Y units when root grid collision is enabled, composes the
/// root and publishes its Q12 forward/push-back heading to all three spheres.
/// An equipped weapon supplies a copied root transform, with fixed collision
/// rotations except for the Gunblade. Borrows one SVECTOR scratch block.
void playerActorUpdateMove(void);

void Gp_EffSprTask81(Task* arg0);

void func_800F91AC(Task* arg0);

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

void func_800FF710(Task* arg0);

/// Sources of a player reload animation; state 5 retains this value until completion.
enum {
    PLAYER_ACTOR_RELOAD_AUTOMATIC  = 0,
    PLAYER_ACTOR_RELOAD_MENU       = 1,
    PLAYER_ACTOR_RELOAD_BATTLE_END = 2,
};

/// Enters the player's reload animation for an automatic, menu or battle-end request.
///
/// loadSelection is 0 for primary or 1 for secondary. Automatic requests refill
/// the selected load; menu requests apply the pending consumable and are ignored
/// during scripted control. Battle-end callers pass 0, play clip 20 and request
/// the corresponding companion-family-1 action. Other requests play clip 14/15;
/// all blend for three normal-rate frames and reset weapon attack effects.
/// Requires live actor/native animation resources, a weapon index in 0..32 and
/// live equipment/session state; family 1 additionally requires its live task.
/// The load selection and source narrow into stateAux and actionValue for state 5.
void playerActorEnterReload(Task* task, s32 loadSelection, s32 reloadSource);

/// Starts scripted movement to a borrowed transform's position.
///
/// Copies XYZ destination in the model root's parent frame, in game-coordinate
/// units; transform rotation is ignored. Clears movement signs, aim offsets and
/// attack effects, enters scripted state 4 and requests collision disablement.
/// `moveAnim` may be NULL; its approach/arrival IDs narrow to u16, with zero
/// selecting each default clip. Payloads need live readable storage only for
/// this call. Requires live GameActor work, weapon effects and session state;
/// the later approach tick requires a live model and animation bank. Returns 0;
/// the message ID is unused.
s32 playerActorMoveTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim);

/// Applies a borrowed displacement and returns the actor's current wall-contact result.
///
/// Adds XYZ in the model root's parent frame, in game-coordinate units; the
/// vector's fourth word is unread. Zero keepControl resets movement, aim and
/// attack state, enters scripted state 1 and sets the pending-motion latch.
/// Collision requests replace the pending byte, keeping their low eight bits.
/// All-enable requests with nonzero X/Z also choose forward/backward movement
/// from the displacement bearing. Does not recompute contacts: returns 1 for
/// any stored non-floor grid contact, otherwise 0. Requires live actor/model,
/// initialized contact storage, weapon effects and session state. Retains no
/// request pointer; message ID and second argument are unused.
s32 playerActorMoveBy(Task* task, s32 unusedMessageId, const GameActorMoveBy* move, s32 unusedSecondArg);

/// Spawns and places the controlled player, with work initialized for its first tick.
///
/// Requires resourceVariant 1..4 and its loaded bank-7 player descriptor/model,
/// initialized heaps/session, and readable transform/options. Copies XYZ in
/// integer game-coordinate units and the signed-halfword yaw (4096 per turn).
/// Selects the model from resourceVariant; unusedCharacterId is ignored.
/// Forwards spawnArg as the task's first payload (current callers pass zero).
///
/// Copies the initial animation and scripted-start choice synchronously. The
/// task retains the options address as its second payload, but the player
/// dispatch, message and weapon-action paths do not read it after this call;
/// neither input record needs to remain live. Registers the task in the player
/// session slot and clears the actor-tick gate. The first tick installs playback,
/// collision and teardown state. Returns NULL on allocation failure, killing a
/// newly spawned task if its work allocation fails; returns the live task otherwise.
Task* playerActorSpawn(const ActorSpawnTransform* spawnTransform, u16 unusedCharacterId, s32 spawnArg, ActorSpawnOptions* options);

/// Refreshes the live player's weapon collision identity from its equipment.
///
/// Replaces the whole key with attack category, weapon index in bits 8..15 and
/// the current weapon-slot item byte. This also clears attack-selector bits.
/// Requires an occupied player task with live GameActor work; does not relink
/// the body or update existing contact keys.
void playerActorUpdateWeaponCollisionKey(void);

/// Installs a borrowed animation-set table and enters scripted playback.
///
/// Clears movement signs, aim offsets and attack effects, selects scripted
/// state 1 and the direct-bank sentinel, and sets normal playback rate. The
/// request selects a reset or a blend for whole normal-rate frames and queues
/// world-collision enablement or disablement. The request is borrowed for this
/// call; its set table, records and the actor's live model/pose storage must
/// remain valid during playback under the child-slot playback contracts.
/// Returns 0. The message ID and second payload word are unused.
s32 playerActorInstallScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

/// Enters the player's normal-mode aim-entry state and starts its child-slot clip.
///
/// Stops movement and turning, resets phase and attack cooldown and enables the
/// controller that blends into the holding clip once all child slots settle.
/// Zero `blendFrames` restarts clip 7 directly; a nonzero value blends from the
/// captured poses for that many whole normal-rate frames (1..2047). The live
/// actor and native clip table must meet the child-slot playback contracts.
void playerActorEnterAim(Task* task, s32 blendFrames);

/// Starts normal-mode aim exit and releases the actor's borrowed lock target.
///
/// Stops displacement, resets phase, selects turn-rate row 2 and the controller
/// returning to locomotion, then blends native clip 8 over six normal-rate
/// frames. Clears the selected node's targeted mark and requests angle decay.
/// Requires live actor/native playback and a live selected target if present.
void playerActorExitAim(Task* task);

void func_800FAA14(Task* arg0);

/// Dispatches the persistent PE-effect controller in bank 6 slot 07.
///
/// Task state must be 0..2: initialization advances to the attachment-effect
/// dispatcher, and state 2 releases the task. Copies the three-entry callback
/// table before dispatch. The active state requires live room/attachment state
/// and a valid attachment id; it tolerates a missing player task. No task work
/// or coordinate body is used by this dispatcher.
void effectControlTask07(Task* task);

/// Emits the hit blast's flame bursts followed by smoke, bank 6 slot 7F.
///
/// Requires owned EffectWork and a coordinate body with a live borrowed parent.
/// The signed low spawn half is the size in parent-coordinate units; active
/// emission requires size >= 2 so its half-size random range is nonzero. The
/// signed high half selects flame ticks: 1 gives one, other values give three
/// times that value, narrowed to s16. Total lifetime is twice the flame phase,
/// also narrowed to s16; positive phases must fit those halfwords.
///
/// Initializes the local transform from the copied spawn offset, composes it
/// while paused, and advances age only while running. Hidden control suspends
/// it; cancellation or completed lifetime releases its work and task. Spawned
/// sprites snapshot placement and offsets and run independently.
void effectControlTask7F(Task* task);

/// Turns the actor's body yaw toward its lock target beyond a planar dead zone.
///
/// `minGroundDistance` narrows to s16 and uses game-coordinate units; callers
/// pass 0..896. The target must lie strictly farther from the weapon aim origin.
/// Limits each turn to the equipped weapon's rate, increased by half for Quick
/// Fire, and wraps yaw to 0..4095 units per turn. A missing target is a no-op.
/// Requires live actor/weapon models, a valid weapon index, initialized scratch
/// and GTE state, and a borrowed target in the world frame beneath the view. The planar
/// absolute values, squares and sum must fit s32. Retains no pointers.
void playerActorAimYawToLock(Task* task, s32 minGroundDistance);

/// Eases the actor's part-2 and part-3 aim pitch toward its selected lock target.
///
/// Angles use 4096 units per turn. Each call limits change to 48 units and
/// absolute pitch to 288/256 units, deriving roll as 3/5 and 2/5 of pitch.
/// Part 2 measures above its local origin with vertical displacement halved;
/// part 3 measures from the equipped weapon's aim origin. A missing target
/// changes nothing. Requires live actor work, model part 2, equipped model 1,
/// a valid live weapon index and a borrowed target in the world frame beneath the view.
/// Scratch/GTE state and the target-delta arithmetic must meet the aim helpers'
/// contracts. No resource is allocated or pointer retained.
void playerActorAimPitchToLock(Task* task);

/// Eases the model's part-6 pitch toward a lock beyond a planar distance threshold.
///
/// Measures from the equipped model's root plus the weapon-indexed local aim
/// offset. `weaponId` must index the 33-row offset table (0..32); the caller
/// supplies the companion's weapon, independently of the player's live weapon.
/// `minGroundDistance` narrows to s16 game units, and equality does not move.
/// Angles use 4096 units per turn: deltas below 32 units are ignored, each step
/// is limited to 48, and pitch must stay within +/-640. A missing lock is a
/// no-op. Requires live actor, equipped model 1 and borrowed target resources
/// plus the pitch-target helper's scratch/GTE and arithmetic contracts.
void playerActorAimPart6PitchToLock(Task* task, s32 weaponId, s32 minGroundDistance);

/// Releases the actor's selected lock target and starts aim-angle decay.
///
/// Requires live GameActor work and, if selected, a live borrowed target node.
/// Clears that node's targeted mark and the actor's pointer without unlinking
/// or freeing the target. Decay is requested even when no target was selected.
void playerActorClearLockTarget(Task* task);

/// Selects an actor's lock target and transfers the targeted mark from its old node.
///
/// Requires live `GameActor` work and a non-NULL target. The target is borrowed
/// and must remain live while selected; a distinct previous target must stay
/// live through this call. Selecting the same node marks it targeted again.
void playerActorSetLockTarget(Task* task, WorldTargetNode* target);

#endif // GAMEPLAY_PRIVATE_PLAYER_ACTOR_H
