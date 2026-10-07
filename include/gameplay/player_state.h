#ifndef GAMEPLAY_PLAYER_STATE_H
#define GAMEPLAY_PLAYER_STATE_H

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Restores the resident player's current HP and MP to their stored maxima.
///
/// Copies both signed-halfword point counts without clamping or recomputing
/// the maxima. Requires the gameplay overlay, but no live player actor.
/// Status effects, maximum values and the serialized backup are unchanged.
void playerStateRestoreFullHpMp(void);

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

/// Initializes and links a companion's forward obstacle-scan capsule.
///
/// Copies the live model root into the probe's independent coordinate. The
/// endpoints share `nearEndpoint`'s X/Y, with Z taken from that vector and
/// `farEndpointZ` (narrowed to s16). Offsets and the fixed 128-unit radii use
/// local game coordinates; the capsule origin is shifted -160 along Z.
/// Requires live GameActor/CompanionWork/model storage and an unlinked probe.
/// Initializes its full contact array, then enables one-contact grid and pair
/// scans on the player-attack list with category 6. The vector is borrowed only
/// for this call; task-owned probe storage stays live until the body is unlinked.
void companionBindCollisionProbe(Task* task, const SVECTOR3* nearEndpoint, s32 farEndpointZ);

/// Returns a companion to its normal idle behavior and selects idle animation set 1.
///
/// Stops movement and turning and clears the behavior phase, idle timer and
/// action counter. Nonzero `resetAnimation` restarts the child slots directly;
/// zero captures their prior poses and blends for four whole normal-rate frames.
/// The live task's `GameActor` work and native animation resources must meet
/// `playerActorResetChildSlots` / `playerActorPlayChildSlotsWithBlend` contracts.
void companionEnterIdle(Task* task, s16 resetAnimation);

/// Enters the player or companion's stopped damage-mode pose used at zero HP.
///
/// Selects native set 18 and hit selector 3, whose callback does not recover.
/// Zero `blendFrames` resets the child slots; otherwise blends with controller 0
/// for that many whole normal-rate frames under the child-slot blend contract.
/// Stops the movement/turn controllers, releases lock-on and queues disabling
/// the two body grid passes. Requires live GameActor/model/native playback;
/// it does not alter HP or remove the task, and animation ticks still run.
void playerActorEnterStoppedPose(Task* task, s32 blendFrames);

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

/// Initializes companion playback from the live save's native animation bank.
///
/// The saved family selects a base bank and the variant adds to it. They must
/// select a loaded non-NULL bank in 1..7. Requires live GameActor/model, pose
/// buffer and slots satisfying `animationInitContext`; installs the borrowed
/// set table and initializes slot state, without selecting a clip or rate.
/// Bank and clip resources remain live throughout playback.
void companionInitNativeAnimation(Task* task);

/// Applies a companion's pending contact damage and starts its hit reaction.
///
/// Enters damage mode, stops movement/turning and selects the clip-end phase
/// controller. Only saved family 1 loses HP, and only with cheat mode off.
/// Subtraction retains its low halfword, interpreted as s16 for the event
/// survival test; an active event keeps lethal HP at 1. Other families still
/// react. Disables the weapon's grid/pair tests, resets its attack and decays
/// tracked aim, then blends set 16 for hit selector 1 or set 17 otherwise over
/// three normal-rate frames. Requires live native model/weapon resources and
/// valid saved selectors; retains the pending hit until recovery clears it.
void companionEnterDamageReaction(Task* task);

/// Aim axes selected by `companionTrackLockTarget`.
enum {
    COMPANION_LOCK_TRACK_YAW   = 1,
    COMPANION_LOCK_TRACK_PITCH = 2
};

/// Steps the companion's aim toward its live, lockable selected target.
///
/// A missing or non-lockable target clears the pointer and requests aim decay.
/// Otherwise only TARGET tracking steps the requested axes. Yaw alone uses
/// zero planar dead zone; any other mask including yaw uses 896 game units.
/// Pitch uses the variant's joint choice and native weapon: parts 2/3, or
/// part 6 beyond a 896-unit dead zone. Requires the aim helpers' live target,
/// model, scratch/GTE and valid saved-variant contracts. Retains no new pointer.
void companionTrackLockTarget(Task* task, s32 trackingAxes);

/// Removes the companion's weapon model and weapon-effect task.
///
/// When equipment slot 1 is occupied, kills it, restores native playback,
/// resets idle set 1, then returns to normal idle with a four-frame blend.
/// Without that model, leaves behavior and playback intact. Independently
/// kills any weapon effect and clears both removed-task pointers. Requires
/// live GameActor/model/native bank resources; keeps the companion task alive.
void companionRemoveEquipment(Task* task);

void func_8010A9D0(Task* arg0);

/// Applies an attack-key damage message to the live save's companion HP.
///
/// Cheat mode leaves HP intact and returns 0. Otherwise uses
/// `damageComputeReceived`'s companion HP/difficulty/key contract, subtracts
/// into signed-halfword HP, and returns 1 if the resulting HP is nonpositive.
/// That path synchronously broadcasts `ACTOR_MESSAGE_RELEASE_HOLD` through
/// the live scene task. Does not clamp HP or apply the event survival floor.
/// The receiver, message ID and second payload are unused message-ABI words.
s32 companionApplyDamage(Task* unusedTask, s32 unusedMessageId, s32 attackKey, s32 unusedSecondArg);

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

/// Restores the live save's companion HP to its stored maximum.
///
/// Copies the signed-halfword maximum without clamping or checking companion
/// presence. Changes only current HP; an active companion task is not required.
void companionRestoreFullHp(void);

/// Clears a companion's pending hit and blends back to normal idle.
///
/// Arms 18 active recovery ticks, resets idle behavior and blends native set 1
/// over four normal-rate frames. Requires live GameActor/native model playback;
/// it does not apply HP damage or change equipment and retains no new pointer.
void companionRecoverToIdle(Task* task);

/// Recovers a player or companion whose damage clip has completed phase 1.
///
/// Other phases are unchanged. Clears the pending hit, arms 18 recovery ticks
/// and returns to normal aim with a 12-frame blend when the retained state is
/// nonzero, or normal locomotion with its ordinary blend when it is zero.
/// Requires live GameActor/native model playback under those entry contracts.
void playerActorFinishDamageReaction(Task* task);

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
/// Starts a companion hold that completes after the requested button presses.
///
/// A nonzero signed-byte recovery timer refuses with 1 and changes nothing.
/// Otherwise enters scripted state 6, stops motion/aim offsets and weapon
/// attacks, copies `request->pressCount` into the signed-halfword state timer
/// and clears the counted presses. The count must fit 0..32767; zero completes
/// on the first tick. Only the count is read, through synchronous dispatch.
/// Requires a receiver implementing state 6 and live native weapon resources.
/// Leaves the player's interaction latch intact; returns 0 on acceptance.
s32 companionAwaitButtonPresses(Task* task, s32 unusedMessageId, const GameActorButtonPressHold* request, s32 unusedSecondArg);

/// Applies companion displacement while preserving the player's interaction press latch.
///
/// Forwards the borrowed request and both ABI words to `playerActorMoveBy`,
/// restoring the latch afterwards and forwarding its wall-contact result.
/// Coordinate, collision-mask, bounds and lifetime contracts follow that helper.
s32 companionMoveBy(Task* task, s32 unusedMessageId, const GameActorMoveBy* move, s32 unusedSecondArg);

/// Starts scripted companion running while preserving the player's interaction press latch.
///
/// Uses `playerActorMoveTo`'s destination/optional-clip contract, then selects
/// state 8 instead of the walking state. Requires a receiver whose scripted
/// dispatcher implements that state and live player/native weapon resources.
/// Transform and clips are read only through dispatch; stored destination and
/// clip IDs drive subsequent ticks. Restores the latch and returns 0.
s32 companionRunTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim);

#endif // GAMEPLAY_PLAYER_STATE_H
