#ifndef GAMEPLAY_PLAYER_STATE_H
#define GAMEPLAY_PLAYER_STATE_H

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Subtracts `amount` MP from the player, draining the remainder when it is insufficient.
///
/// `amount` is a nonnegative count of Parasite Energy points. Returns 1 when
/// all points were available (including a zero request), otherwise sets MP
/// to zero and returns 0. Used for hostile drains as well as spending;
/// changes neither maximum MP nor HP and retains no pointers.
s32 playerStateSpendMp(s32 amount);

/// Applies or clears a mask of timed player ailments.
///
/// Zero `clearEffects` applies recognized bits independently, respecting each
/// equipment resistance and refreshing its timer to 600 active status updates.
/// Darkness clears lock-on; paralysis clears the pending hit; poison arms
/// damage for the next status update; confusion randomizes its next direction interval.
/// Bit 0x20 is timed but its effect identity remains unproven. Any nonzero
/// `clearEffects` clears the mask from the stored status byte without directly
/// resetting timers or presentation. Applying requires a live player task and GameActor;
/// clearing requires only the live player status. Retains no pointers.
void playerStateSetStatusEffects(s32 clearEffects, s32 statusMask);

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2);

/// Returns a companion to its normal idle behavior and selects idle animation set 1.
///
/// Stops movement and turning and clears the behavior phase, idle timer and
/// action counter. Nonzero `resetAnimation` restarts the child slots directly;
/// zero captures their prior poses and blends for four whole normal-rate frames.
/// The live task's `GameActor` work and native animation resources must meet
/// `playerActorResetChildSlots` / `playerActorPlayChildSlotsWithBlend` contracts.
void companionEnterIdle(Task* task, s16 resetAnimation);

void Gp_StopPlayerAnim(Task* arg0, s32 arg1);

/// Sets the companion's delay before its next idle decision, in active behavior ticks.
///
/// `task->work` must hold a live `GameActor` with a non-NULL `companionWork`.
/// Stores `baseTicks + (rand() & randomMask)` in the signed 16-bit timer.
/// Callers must keep that sum in 0..32767; the mask selects random bits.
/// Updates that do not run the companion's normal behavior do not advance
/// the timer.
void companionSetDecisionDelay(Task* task, s32 baseTicks, s32 randomMask);

/// Resolves player/companion body contacts into pushback and a pending damage hit.
///
/// `contacts` must supply the actor's full 18-entry motion-contact table, with
/// receiving-body indices 0..2. Occupied enemy-body contacts selected by the
/// body-id response table supply the deepest overlap when gridResponse is zero;
/// category-4 attacks and category-5 hazards select damage while no hit is
/// pending. Hazard IDs other than 2..4 do not latch a hit region.
/// Measures overlaps in cached composition space, converts the deepest heading
/// into room space, and leaves the root displaced by one quarter of the overlap.
/// The normalized pushback heading is measured using the full displacement.
/// Requires live actor/model/grid-view transforms, initialized GTE/scratch
/// state and 64 free scratch bytes. Products and squared lengths must fit s32.
/// Borrows the contacts for this call and leaves their contents intact.
void playerActorResolveBodyContacts(Task* task, const WorldCollisionContact* contacts);

void func_8010BFCC(Task* arg0);

void func_8010B9A4(Task* arg0);

void Gp_TrackAllyLockTarget(Task* arg0, s32 arg1);

void Gp_EndPlayerActorTask(Task* arg0);

void func_8010A9D0(Task* arg0);

s32 Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_8010B2A0(s32 arg0, s32 arg1);

/// Indices selecting companion behavior distributions by remaining HP.
enum {
    COMPANION_HEALTH_BAND_ABOVE_HALF      = 0,
    COMPANION_HEALTH_BAND_ABOVE_QUARTER   = 1,
    COMPANION_HEALTH_BAND_AT_MOST_QUARTER = 2
};

/// Returns the live save's companion HP band (0 above half, 1 above a quarter, 2 otherwise).
///
/// Half and quarter thresholds use signed right shifts of maximum HP. Band 1
/// covers HP above the quarter threshold through the half threshold; band 2
/// includes equality at the quarter threshold. Zero or negative HP also belongs
/// to band 2 for a nonnegative maximum.
s32 companionGetHealthBand(void);

void func_8010C180(Task* arg0);

void func_8010ABD4(Task* arg0);

/// Returns the XZ distance from a coordinate's local translation to the live player.
///
/// `coord` and the player model's root must use the same parent frame and signed
/// game-coordinate units. Neither transform is composed or rotated; Y is ignored
/// by the length calculation. The player task and its TMD body must be live.
/// Differences, absolute values, squares and their sum must fit s32. Requires
/// an initialized scratch stack with 16 free bytes, released before return;
/// retains no pointers and performs no bounds or overflow checks.
s32 companionGetPlayerPlanarDistance(const GfxCoord* coord);

/// Returns the shortest signed body-yaw turn toward a point, in 1/4096 turns.
///
/// Measures the point's XZ bearing from the live model root's local translation;
/// `targetPoint` must use that root's parent frame and game-coordinate units.
/// The result narrows through s16; normal headings yield -2048..2048, retaining
/// both half-turn endpoints. Requires live GameActor/model storage, initialized
/// scratch state and 28 free bytes (16 for the delta, 12 for the turn helper).
/// XYZ differences must fit s32. The point is borrowed for this call and no
/// rotation is changed.
s32 playerActorGetTurnToPoint(Task* task, const VECTOR3* targetPoint);

/// Steps the actor's body yaw toward a point by at most 64 units per call.
///
/// Angles use 4096 units per turn and the resulting yaw wraps to 0..4095.
/// The point uses the model root's parent frame and game-coordinate units;
/// requires live GameActor/model storage and initialized scratch state with 32
/// free bytes (20 for this block, 12 for the turn helper). XYZ differences must
/// fit s32. Borrows the point only for this call.
void playerActorTurnBodyTowardPoint(Task* task, const VECTOR3* targetPoint);

/// Steps the actor's aim yaw toward a point by at most 32 units per call.
///
/// Measures from model part 4 beneath the view node; `targetPoint` uses that
/// node's frame and game-coordinate units. Angles use 4096 units per turn.
/// A step is accepted only if the resulting relative aim yaw has magnitude
/// below 416; an already out-of-range yaw is not clamped. Requires a live actor,
/// model part 4, view hierarchy, initialized GTE/scratch state and 104 free
/// bytes plus 48 for the relative-transform helper (152 free bytes at peak).
/// XYZ differences must fit s32. Borrows the point for this call.
void playerActorTurnAimTowardPoint(Task* task, const VECTOR3* targetPoint);

void func_8010C980(void* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3, s32 arg4, s32 arg5);

// Message handlers addressed by the companion overlays' dispatch tables.
/// Takes scripted companion control and plays a clip from an indexed bank.
///
/// The borrowed request's source index must select a loaded, non-NULL companion
/// bank (0..7). A different set table reinitializes playback and updates the
/// stored bank index. Enters scripted state 1, stops motion/aim offsets and
/// weapon attacks, sets normal playback rate and requests the chosen world
/// collision participation. Reset restarts the child slots; any nonzero blend
/// choice blends with controller 1 for the requested whole normal-rate frames.
/// The clip, blend and live model/pose storage must meet the child-slot playback
/// contracts. The request lives through dispatch; the bank and clips stay live
/// throughout playback. Returns 0; both unused words retain the message ABI.
s32 companionPlayScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

/// Starts a scripted companion turn while preserving the player's interaction press latch.
///
/// `playerActorTurnToYaw` supplies the turn/animation behavior and payload
/// contract. The borrowed transform's yaw uses 4096 units per turn; other
/// components are ignored. Restores PlayerStatus.interactionPressed after the
/// call and returns 0. Both unused words are forwarded under the message ABI.
s32 companionTurnToYaw(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg);

/// Restores a companion's native animation bank and normal idle behavior.
///
/// Folds model part 1's animated XZ translation through the root basis into
/// the root position, clears that part's XZ offset, and records the resulting
/// position. Native bank selection uses the live save's companion type/variant.
/// Reinitializes playback at normal rate and enables body world collision.
/// A changed set table restarts idle slots; an unchanged table blends to idle
/// over four normal-rate frames. Requires live GameActor/model/native resources
/// and valid saved companion selectors; returns 0 and ignores all payload words.
/// This also handles companion aliases of replacement-animation messages.
s32 companionEndScriptedMotion(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Starts scripted companion movement while preserving the player's interaction press latch.
///
/// Copies the borrowed transform's XYZ destination in the root's parent frame;
/// rotation is ignored. `moveAnim` may be NULL and follows `playerActorMoveTo`'s
/// default-clip and lifetime contract. Restores PlayerStatus.interactionPressed
/// after starting the move and returns 0. The unused message ID is forwarded.
s32 companionMoveTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim);

/// Installs companion scripted playback while preserving the player's interaction press latch.
///
/// Uses `playerActorInstallScriptedAnimation`'s borrowed-table, playback and
/// collision contract. The request is needed through dispatch; its table and
/// clips remain live during playback. Restores PlayerStatus.interactionPressed
/// after the call and returns 0. Both unused words retain the message ABI.
s32 companionInstallScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

/// Copies raw words into the live save's selected companion animation-bank extension.
///
/// Selects the bank by saved companion type/variant, independently of the
/// receiver's current scripted table. The bank must be loaded and writable.
/// `AnimationBankCopyRequest` supplies the source extent and lifetime contract:
/// 1..32 words overwrite the extension starting at index 47, nonpositive counts
/// copy nothing and return 0, and larger counts return 1 without copying.
/// Copies forward and does not start playback; copied clip data remains borrowed.
/// The task, message ID and second payload are unused but retain the message ABI.
s32 animationCopyCompanionBankExtension(Task* unusedTask, s32 unusedMessageId, const AnimationBankCopyRequest* request, s32 unusedSecondArg);
s32 func_8010C75C(Task* task, s32 msgId, GameActorButtonPressHold*, s32 unusedSecondArg);
s32 Gp_MoveActorByKeep(Task* task, s32 msgId, GameActorMoveBy*, s32 unusedSecondArg);
s32 func_8010C708(Task* task, s32 msgId, ActorTransform* transform, GameActorMoveAnim* moveAnim);

#endif // GAMEPLAY_PLAYER_STATE_H
