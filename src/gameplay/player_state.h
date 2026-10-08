#ifndef GAMEPLAY_PRIVATE_PLAYER_STATE_H
#define GAMEPLAY_PRIVATE_PLAYER_STATE_H

#include "types.h"

#include "actor.h"
#include "gameplay/actor_spawn_types.h"

#include "main/task_types.h"

Task* Gp_SetupAllyWeapon(void);

/// Spawns the selected companion model and initializes its actor and probe work.
///
/// companionType is family 1..3. Family 1 uses the live save's resource variant
/// 1..4; other families select their fixed descriptor. scheduleIndex is the
/// family-2 schedule (0..10), or zero for other families. Requires that family's
/// loaded model/texture resources and primary heap. Position uses whole world
/// units and yaw copies the low 16 bits unchanged (4096 units per turn).
/// Both input records need live storage only during this call: animation id is
/// copied and companion callbacks do not read the retained options payload.
/// Returns the registered companion task, or NULL after spawn/allocation failure.
/// GameActor owns a separate CompanionWork allocation; its eventual reclamation
/// and the partial-allocation failure lifetime are unproven.
Task* companionSpawnActor(const ActorSpawnTransform* spawnTransform, u16 companionType, s32 scheduleIndex, ActorSpawnOptions* options);

/// Advances the live player's timed status effects by one normal-mode update.
///
/// Requires live player GameActor/session state. Expired halfword durations
/// clear their flags. Poison deals one HP at signed-byte intervals of 120 ticks
/// stopped, 20 running, or 60 otherwise; its damage tick precedes expiry.
/// Berserker's duration pauses in either active attachment mode. Timers advance
/// only when called, and no new pointer is retained.
void playerStateTickStatusEffects(Task* task);

/// Applies a received attack's status or tint effect to the live player.
///
/// The low byte of `reaction` selects `GAME_ACTOR_REACTION_*`; ordinary and
/// unsupported codes do nothing. Accepted ailments refresh 600 active ticks
/// unless equipment resists them. Reaction 4 emits only its tint; reaction 9
/// applies the still-unidentified timed status bit 0x20 without effective
/// equipment resistance. Requires live player
/// work and presentation resources; does not apply HP loss or choose a hit clip.
void playerStateApplyReactionEffect(Task* task, s32 reaction);

/// Adds confused direction input and occasionally changes the player's lock target.
///
/// Call during normal mode while confusion is active, before deriving movement
/// from `padHeld`. Every 10..41 active calls selects or clears synthetic d-pad
/// directions and may release or acquire a combat target. Directions are ORed
/// into input only while the session holds a d-pad direction. Requires live
/// player/session and combat target resources; does not own selected targets.
void playerActorApplyConfusionInput(Task* task);

/// Applies player HP damage with equipment modifiers and fatal-hit handling.
///
/// `damagePoints` is signed HP loss, narrowed to s16 by callers with no sign
/// clamp; negative values reverse the HP/MP changes. Holy Water removes the
/// arithmetic-shift quarter; MP Generation credits one MP per five reduced HP
/// before survival handling, capped at maximum MP. Impact resistance leaves one
/// HP on a lethal hit when starting HP is at least five. Active events likewise
/// retain one HP. Otherwise nonpositive resulting HP acquires a menu hold and
/// returns 1; every surviving path returns 0. Requires live player/session state
/// and a live player model when the resistance burst is emitted.
s32 playerStateApplyHpDamage(s16 damagePoints);

/// Advances a pending hit's flashes on player model parts 3, 2, then 1.
///
/// Reaction 5 emits flashes of size 800 game units six callback ticks apart,
/// starting on the first call. Phase 0 initializes the timer and count. Before
/// the third flash, clears the hit and resumes normal aim/locomotion with 18
/// recovery ticks. Requires live GameActor/model parts 1..3, native playback and
/// effect resources; the damage-mode dispatcher stops calling after recovery.
void playerActorTickHitFlashes(Task* task);

/// Presents a poison hit at its receiving motion body and waits for clip completion.
///
/// Phase 0 emits tinted puffs and enters phase 1; phase 2 returns to normal
/// aim/locomotion with 18 recovery ticks. The clip controller supplies that
/// final phase. Body indices 0..2 select their live collision coordinates;
/// body 0 uses a local Y offset of -400 game units. Requires live player/model,
/// native playback, GTE and an initialized scratch stack with 56 free bytes
/// (8 for its vector and 48 for relative placement). Releases its vector before
/// return; status duration and HP loss were applied when the hit began.
void playerActorTickPoisonHit(Task* task);

/// Resolves player body contacts and presents a pending collision hit.
///
/// Cheat mode or nonzero signed-byte recovery suppresses the whole check.
/// Requires live player GameActor/model, full collision contacts and native
/// weapon/animation resources under `playerActorResolveBodyContacts`' contract.
/// A hit stops the weapon, applies HP/status damage when HP was positive, then
/// blends set 16 for body part 4 or 17 otherwise over three normal-rate frames
/// and starts positional hit sound 6 or 7. Retains the hit until recovery;
/// contact pushback can occur without a damage hit.
void playerActorCheckContactDamage(Task* task);

/// Clears the actor's pending collision hit region, reaction and HP damage.
///
/// Requires live GameActor task work. Restores the ordinary reaction code;
/// the saved hit-body index is meaningful only while a hit region is pending.
void playerActorClearPendingHit(Task* task);

/// Emits three blast recipes on the live player at seven-callback intervals.
///
/// A bodyless task at state 0 initializes its packed counter and emits at once;
/// state 1 continues it. The low two bits of spawnArg1 choose size 192..480
/// game units in steps of 96 and recipe variant 1..4. Bits 8..11 of the counter
/// select the part; its low nibble counts delay ticks. Parts are 2, 3, then 1: teardown on
/// the last call clears the counter before placement is read again. Requires
/// deferred task collection, a live player/model through its spawned effects,
/// and initialized effect/GTE/scratch resources. Does not change player damage.
void effectPlayerBodyBlastTask(Task* task);

/// Emits a tinted-puff recipe on player model part 3 and removes its bodyless task.
///
/// Low16 of spawnArg1 plus one narrows to the recipe's signed-halfword extra
/// puff count. Use 0..32766 for nonnegative counts; recipe 2 always emits its
/// three base puffs. Requires a live player/model through the spawned effects
/// and initialized effect/GTE/scratch resources. Player damage is unchanged.
void effectPlayerBodyPuffTask(Task* task);

#endif // GAMEPLAY_PRIVATE_PLAYER_STATE_H
