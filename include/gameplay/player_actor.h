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
/// `D_80112E04[field_22][1]` to `playerActorIsSlotAdvancingLinearly`.
extern u8 D_80112E04[][2];

/// u16 table indexed by `playerActorInitWeaponCollision` weaponId: the reach a weapon of that
/// attach id adds to the shape's `ends[1]` to give its `ends[0]`.
extern u16 D_80112F60[];

extern u16 Gp_WeaponIdBase[2];

extern AnimationBank* Gp_PlayerAnimBlkTbl[34];

/// Initializes and links the equipped weapon's collision capsule for a player-type actor.
///
/// Requires live GameActor work; a missing equipment slot 1 leaves collision
/// state unchanged. Copies the weapon root coordinate, rotates its cached
/// frame by a quarter turn about X and installs the actor-owned capsule and
/// six-contact table on the player-attack list. The capsule and copied node
/// remain owned by the actor until the body is unlinked.
///
/// `weaponId` is a weapon/reach row in 0..32, packed into key bits 8..13;
/// `attackRow` supplies the low attack identity byte. Row 13 widens the far
/// radius for spread fire. Radius also depends on the player's equipped
/// weapon even when this initializes a companion. The model, lookup tables
/// and initialized scratch stack must be live; no bounds are checked.
void playerActorInitWeaponCollision(Task* actorTask, s32 weaponId, s32 attackRow);

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

/// Rig joint rows and the second model pair used at player/companion initialization.
enum {
    PLAYER_ACTOR_ATTACHMENT_PLAYER_RIG    = 1,
    PLAYER_ACTOR_ATTACHMENT_COMPANION_RIG = 5,
    PLAYER_ACTOR_ATTACHMENT_SECOND_PAIR   = 1,
};

/// Spawns one model of a player or companion's paired rig attachments.
///
/// attachmentIndex is 0 or 1; pairVariant is 0 or 1 within a four-descriptor
/// group. rigIndex selects a joint row in 0..5 and rigIndex + resourceVariant - 2
/// must be in 0..7. Current callers use rig 1 for the player and rig 5 for the
/// armed companion, with resource variants 1..4 and pair variant 1. The selected
/// joint must exist in the live actor model and the bank-7 descriptor/model
/// must be loaded. Companion callers temporarily supply their variant through
/// `gPlayerStatus.resourceVariant`.
///
/// Returns NULL on spawn failure. The new task records actorTask as its parent
/// and borrows the rig joint; both must remain live until attachment teardown.
/// Preserves draw flags on the child's first update, selects the actor's texture
/// bank and rebuilds both primitive halves. Does not replace an existing slot.
Task* playerActorSpawnAttachment(Task* actorTask, s32 attachmentIndex, s32 rigIndex, s32 pairVariant);

/// Rebuilds the player's equipped weapon model and restores native playback.
///
/// Requires a live task in the player session slot and character ID 1..2;
/// NULL work returns NULL.
/// Spawns a weapon model under attachment 1 when present and initializes its
/// collision body. Hypervelocity, Hammer and Pyke also create a missing owned
/// persistent effect, parented to the player task; only a successful new effect
/// resets attack effects here. Restores the character/weapon animation bank,
/// enters locomotion, clears reload-effect suppression and enables collision
/// updates and view triggers even when allocation fails. Returns equipment slot
/// 1, which may be NULL. Call after removing old equipment; an existing model
/// is not released before its slot is replaced. Loaded model, animation and
/// weapon-overlay resources must remain live until their tasks are removed.
Task* playerActorRestoreEquipment(void);

/// Resets weapon attack effects, requests aim decay and disables weapon collision.
///
/// Requires live GameActor work and a valid weapon index (0 no weapon, 1..32).
/// Hypervelocity cancels its charge and stops all charge sounds; Hammer chooses
/// off/idle glow from its secondary load; Pyke resets firing to off/idle and
/// stops the player or companion fire tail when its persistent effect exists.
/// Remaining-load checks use the live saved weapon supplies without spending
/// them. Existing effect tasks are borrowed and must be live; none is freed.
/// The final argument is ignored and retained for the calling convention.
void playerActorResetWeaponAttack(Task* task, s32 weaponId, s32 unusedArgument);

/// Starts a scripted turn toward a borrowed transform's yaw.
///
/// Reads only `rot.vy`, in 4096 units per turn, and copies it as the target yaw.
/// Clears movement signs, aim offsets and attack effects, enters scripted
/// state 2, marks motion pending and queues collision disablement. Starts clip
/// 5 for a negative shortest turn or clip 6 otherwise, capturing the old pose
/// with zero blend time. Requires live GameActor work, native animation/model
/// resources and initialized scratch state. The transform is borrowed only for
/// this call; the second payload is unused. Returns 0.
s32 playerActorTurnToYaw(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg);

/// Marks the nearest eligible room contact with the equipped weapon's impact effect.
///
/// `contacts` is the initialized six-element weapon-contact table, including
/// its LAST marker. Any enemy-body contact suppresses room impacts. Otherwise
/// chooses the nearest grid contact by XYZ Manhattan distance from the weapon's
/// composed origin, among surfaces permitting weapon impacts in the active room.
/// `weaponCoord` must already be composed in the contact points' coordinate frame.
/// Absolute differences must fit s32; their sum must be below 0x7FFFFFFF.
///
/// Returns 1 for a chosen point, even when Javelin suppresses spawning or an
/// effect allocation fails, else 0. A non-NULL `impactCoordOut` receives only
/// its three cached translation words, in game-coordinate units, with random
/// 0..7 jitter on each axis; failure leaves it unchanged. It may alias the weapon
/// node. No output rotation, parent or cache stamp is installed.
///
/// Requires live player/room state, surface tables, and initialized scratch/GTE
/// state. The temporary spawn coordinate's rotation is left as scratch contained
/// it, and the effects retain its address and offset after release; spawn-time
/// orientation remains unproven. Preserve this placement behavior.
s32 playerActorSpawnWeaponImpact(const WorldCollisionContact* contacts, const GfxCoord* weaponCoord, GfxCoord* impactCoordOut);

/// Reports whether a slot has neither settled at a boundary nor followed a control jump.
///
/// Returns 1 when both result flags are clear, otherwise 0. Reads the flags
/// left by the most recent playback operation; it does not tick the slot or
/// test its rate. `ANIMATION_SLOT_REACHED_END` alone does not change the result.
/// `task->work` must be a live `GameActor`, and `slotIndex` must select a slot
/// in its active prefix. No bounds are checked. The last two arguments are
/// ignored and retained for the calling convention. No pointer is retained.
s32 playerActorIsSlotAdvancingLinearly(Task* task, s32 slotIndex, s32 unusedFirstArg, s32 unusedSecondArg);

/// Alternate-fire selector stored in bit 14 of the weapon body's packed attack key.
enum { PLAYER_ACTOR_WEAPON_ATTACK_ALTERNATE = 0x4000 };

/// Replaces the weapon body's attachment and alternate-fire selector bits.
///
/// `task->work` must be a live GameActor. Both selectors must be 0 or 1:
/// `attachmentAttack` sets bit 15, selecting attachment damage rows;
/// `alternateAttack` sets bit 14, selecting alternate critical chance and
/// hit-effect variants. Other key bits are retained. Does not enable collision
/// or initialize the key. Callers must choose a matching low attack row.
void playerActorSetWeaponAttackFlags(Task* task, s32 attachmentAttack, s32 alternateAttack);

/// Restarts a player or companion actor's child slots on a selected animation set.
///
/// Visits slots 1 through `animationSlotCount - 1`, leaving slot 0 intact;
/// a count at most 1 performs no work. Each reset selects the same-numbered
/// track and coordinate in the context's set table, clears boundary state and
/// flags, then replaces normal rate with the actor's signed `animationRate`
/// in sixteenths of a frame. Does not write a pose or capture a transition.
///
/// `task->work` must be a live `GameActor` with initialized animation context.
/// The active prefix must fit its slots and the model's coordinates and tracks.
/// `setIndex` must fit u16 and select a loaded set other than
/// `ANIMATION_SET_BUFFERED_POSE`. The borrowed table, clips, model and actor
/// storage must remain live during playback. Bounds and record requirements
/// follow `animationResetSlot`; this wrapper performs no validation.
void playerActorResetChildSlots(Task* task, s32 setIndex);

/// Returns a completed weapon attack to scripted attack waiting or ordinary aiming.
///
/// The scripted aim-request bit re-enters scripted state 10 after clearing
/// movement and attack effects; otherwise enters normal aim locomotion with a
/// three-frame pose blend. Requires live GameActor, session, equipment effects
/// and native model/animation resources under the corresponding entry contracts.
void playerActorFinishWeaponAttack(Task* task);

/// Advances the player or companion actor's child animation slots and applies their poses.
///
/// Visits slots 1 through `animationSlotCount - 1`, leaving slot 0 intact;
/// a count at most 1 performs no work. Each slot consumes its own signed rate
/// in sixteenths of a frame, with the death-playback adjustment. `task->work`
/// must hold a live, initialized `GameActor`; the active prefix must fit its
/// slot and pose arrays. Coordinate, track, record, encoding, scratch and GTE
/// requirements follow `animationTickSlotPose`. Borrowed resources must remain
/// live during playback; no bounds are checked and no pointer is retained here.
void playerActorTickChildSlots(Task* task);

/// Channels and halfword packing used by `playerActorQueryWeaponLoads`.
enum {
    PLAYER_ACTOR_WEAPON_LOAD_PRIMARY         = 1,
    PLAYER_ACTOR_WEAPON_LOAD_SECONDARY       = 2,
    PLAYER_ACTOR_WEAPON_LOAD_SECONDARY_SHIFT = 16,
};

/// Returns selected remaining loads of the equipped weapon without consuming them.
///
/// `loadMask` selects primary (bit 0) and secondary (bit 1); other bits are
/// ignored. The primary quantity occupies the low 16 bits and secondary the
/// high 16 bits; an unselected channel contributes zero. Requires a live equipped
/// weapon index 1..32 and initialized saved supplies. Returns zero for an empty
/// selection or empty selected loads and makes no inventory change.
s32 playerActorQueryWeaponLoads(s32 loadMask);

/// Enters ordinary player locomotion using the current movement and turn inputs.
///
/// Selects idle, in-place turning, backward walking, forward walking or running.
/// The saved walk/run preference, run button, run restriction and presence of
/// the equipped weapon select the forward clip. Resets the normal-mode state,
/// animation controller, phase and idle timer and sets movement/turn-rate indices;
/// movement and turn signs are retained. `resetAnimation != 0` restarts child
/// slots directly; zero captures their previous poses and blends for four whole
/// normal-rate frames. The live actor and native animation resources must meet
/// `playerActorResetChildSlots` / `playerActorPlayChildSlotsWithBlend` contracts.
void playerActorEnterLocomotion(Task* task, s16 resetAnimation);

/// Returns the shortest signed turn from the current angle to the target angle.
///
/// Angles use 4096 units per turn. Current angle is wrapped to 0..4095;
/// target uses that range or the signed heading range -2048..2047.
/// Returns -2048..2048, choosing the wrapped candidate on a half-turn tie.
/// Wider inputs narrow to s16 at the call boundary. Requires 12 bytes on the
/// initialized scratch stack, released before return; retains no pointers.
s16 playerActorShortestTurn(s16 currentAngle, s16 targetAngle);

/// Applies the player or companion actor's current movement to its model root.
///
/// Requires live GameActor work and root coordinates. Mode 0 clears velocity;
/// modes 1..3 and 5..7 move along the normalized root forward axis only while
/// animation slot 1 has a record. Mode 4 applies a forward step then a circling
/// step about a live lock target. Movement and turn signs select direction;
/// the speed row divides the Q12 axis to produce game-coordinate displacement.
/// Other modes reuse the existing velocity. Updates velocity and local root
/// translation without marking composition dirty.
///
/// Tables, playback, scratch stack and SDK normalization must be initialized.
/// Circling requires nonzero XZ Manhattan distance to the lock target and
/// intermediate arithmetic within signed word range. Retains no pointers.
void playerActorStepMovement(Task* task);

/// Removes the player's equipped model tasks and persistent weapon effect.
///
/// Requires a live player task in the session slot; NULL work returns 0.
/// Kills equipment slots 0 and 1 and the weapon-effect task in that order,
/// clears their pointers and unlinks the weapon collision body, returning 1.
/// Actor, attachment models, inventory and equipment selection remain live.
/// Child exit callbacks and taskKill's deferred/immediate release rules apply;
/// callers selecting immediate teardown must satisfy its execution-list contract.
s32 playerActorRemoveEquipment(void);

/// Rebuilds player-type actor facing and applies its aim offsets to model parts.
///
/// Requires live GameActor work, model parts 0..6, and a turn-rate index in
/// 0..3. Adds the signed rate to body yaw and wraps to 0..4095 units per turn,
/// rebuilding and normalizing the root rotation while retaining translation.
/// Decaying aim offsets ease toward zero until tracking switches off.
/// Applies pitch/roll to parts 2/3, aim yaw to part 4, and pitch to part 6;
/// those parts' composition stamps are invalidated. The root stamp is retained.
/// Angles use 4096 units per turn. No pointers are retained.
void playerActorUpdateFacing(Task* task);

/// Selected held fire input, with primary taking precedence when both are held.
enum {
    PLAYER_ACTOR_ATTACK_BUTTON_NONE      = 0,
    PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY   = 1,
    PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY = 2,
};

/// Updates and returns the actor's held fire-input selector (0 none, 1 primary, 2 secondary).
///
/// Uses the actor's remapped held buttons outside scripted mode. Scripted mode
/// reads controller port 0 directly: layout 2 uses square/triangle, other layouts
/// use R1/R2. Primary wins when both are held. The live `GameActor` work is
/// updated on every call; this reports input, independent of collision results.
s8 playerActorReadAttackButton(Task* task);

/// Writes a point's displacement from a coordinate's local translation.
///
/// `point` must be in the node's parent frame, in signed game-coordinate units.
/// Subtracts `coord->coord.t` without rotating or composing the node. Reads and
/// writes exactly three 32-bit components; a fourth SDK vector word is neither
/// required nor accessed. Inputs are borrowed for this call, and `delta` may be
/// the same object as `point`. Component differences must fit s32.
void playerActorGetPointDelta(const GfxCoord* coord, const VECTOR3* point, VECTOR3* delta);

/// Returns the planar length of two signed coordinate components using `SquareRoot0`.
///
/// Takes absolute values, squares X and Z and square-roots their sum. Inputs and
/// result use the same game-coordinate units. The absolute values, squares and
/// sum must fit s32; no overflow check or scale conversion is performed.
s32 playerActorPlanarLength(s32 x, s32 z);

/// Captures the prior child-slot poses and selects an animation set with zero blend time.
///
/// Visits slots 1 through `animationSlotCount - 1`, leaving slot 0 intact;
/// a count at most 1 performs no work. Capture ticks at each slot's previous
/// rate before the actor's `animationRate` is applied to the selected clip.
/// Uses the actor's native `animationSets` table and retains the capture tick's
/// flags and boundary latch. `unusedArgument` is ignored for ABI compatibility.
///
/// The low 16 bits of `setIndex` must select a loaded set other than
/// `ANIMATION_SET_BUFFERED_POSE`. Actor, model, slot, pose-buffer, borrowed-table,
/// record, encoding, scratch and GTE requirements are those of
/// `playerActorPlayChildSlotsWithBlend`, with `blendFrames` fixed at zero.
void playerActorPlayChildSlots(Task* task, s32 setIndex, s32 unusedArgument);

/// Applies animation-boundary transitions to the player or companion's action state.
///
/// Samples slot 1 without ticking playback. Aim entry waits for every child
/// slot to settle, then blends to holding clip 9 over five normal-rate frames.
/// Other controllers return to locomotion/aim, advance the action phase or set
/// its completion marker according to the slot's linear-run and boundary flags.
/// Requires live GameActor/model/native-bank resources and an initialized slot
/// 1; the active child-slot prefix must fit storage. Child-slot blending follows
/// its playback contract. No resource is allocated or pointer retained.
void playerActorTickAnimationState(Task* task);

/// Tracks the selected lock with body yaw and weapon-specific joint elevation.
///
/// Missing or no-longer-lockable targets request aim decay; the latter also
/// clears the borrowed target's mark and pointer. Active tracking turns beyond
/// 384 game units (512 for Gunblade), then adjusts torso pitch or Gunblade roll.
/// Requires live actor, model, equipped weapon and target resources under the
/// aim helpers' coordinate, scratch, GTE and arithmetic contracts. Allocates
/// nothing and does not select a replacement target.
void playerActorTrackLockTarget(Task* task);

/// Launcher groups, actor variants and packed payload fields for grenade spawning.
enum {
    PLAYER_ACTOR_GRENADE_PLAYER           = 0,
    PLAYER_ACTOR_GRENADE_COMPANION        = 1,
    PLAYER_ACTOR_GRENADE_M4A1             = 0,
    PLAYER_ACTOR_GRENADE_PISTOL           = 1,
    PLAYER_ACTOR_GRENADE_MM1              = 2,
    PLAYER_ACTOR_GRENADE_WEAPON_SHIFT     = 8,
    PLAYER_ACTOR_GRENADE_MUZZLE_ROW_SHIFT = 16,
    PLAYER_ACTOR_GRENADE_COMPANION_SHOT   = 1 << 20,
};

/// Spawns a grenade projectile at the actor's equipped weapon muzzle.
///
/// launcherIndex is 0 M4A1 grenade, 1 Grenade Pistol, or 2 MM1; actorVariant is
/// 0 player, or 1 for the armed companion with launcher 2. These select bank-7
/// descriptors 96, 100, 104 or 105. Requires live GameActor work, equipment slot
/// 1 and the selected projectile package/model. spawnArg is forwarded unchanged:
/// the low byte is the ammunition index, the next byte the weapon identity, and
/// bits 16..19 select that package's muzzle-offset/speed row.
/// Bit 20 marks a companion shot in the shared Grenade Pistol/MM1 shell code;
/// the M4A1 initializer uses a fixed muzzle offset and ignores the row selector.
///
/// Returns NULL on spawn failure. Records actorTask as the task parent and
/// borrows its weapon root for initial placement; the projectile's first state
/// converts that muzzle pose into its own view-parented flight coordinate.
Task* playerActorSpawnGrenadeProjectile(Task* actorTask, s32 actorVariant, s32 launcherIndex, s32 spawnArg);

/// Records one weapon use in the live save when its counter is below 99,999.
///
/// `weaponId` is the 1-based weapon index, 1..32 (item ID - 0x7F); no weapon
/// index 0 is invalid. The saved counter is indexed by `weaponId - 1`. A counter
/// already at or above the limit is unchanged. Cheat mode does not suppress this
/// entry point. This call retains no arguments.
void weaponRecordUse(s32 weaponId);

/// Spawns a selected weapon model beneath an attachment task's root.
///
/// characterId indexes the four descriptor bases in 0..3; current callers use
/// 2 for the armed companion and weaponId 1..4 for its loadout. The selected
/// bank-7 descriptor at base + weaponId - 1 and its model must be loaded.
/// weaponId 0 or spawn failure returns NULL. Requires a live parent model even
/// for weaponId 0; the parent root is read before testing that sentinel.
///
/// Forwards spawnArg unchanged as the first task argument. Records parentTask
/// as the task parent, borrows its root coordinate and requests draw-flag
/// clearing on the child's first update. Keep the parent/model and descriptor
/// code loaded while the child uses them. Does not replace an equipment slot.
Task* playerActorSpawnWeaponModel(Task* parentTask, s32 characterId, s32 weaponId, s32 spawnArg);

/// Returns 1 for any non-floor grid contact in the actor's complete contact array.
///
/// Scans all 18 stored contacts, including entries after a contact end marker;
/// it does not query or update collision. The live `GameActor` work must have
/// its contact array initialized. Floor and non-grid contacts return 0 alone.
s32 playerActorHasWallContact(Task* task);

/// Turns toward the scripted destination, travels to it and plays the arrival clip.
///
/// Requires live GameActor/model/playback state and initialized scratch storage.
/// Destination XYZ uses game-coordinate units in the root's parent frame; yaw
/// uses 4096 units per turn and changes by at most 64 per tick. Phases 0/1 turn
/// before phase 2 travels. Starts walk mode 1; the run-state wrapper replaces
/// the speed and approach clip once turning finishes. Zero actionArgument selects
/// walk clip 2 (19 without equipment slot 1); zero actionValue selects idle 1.
/// Both clips blend over five normal-rate frames and must fit the loaded bank.
///
/// Arrives when both absolute X/Z errors are below 105, ignoring Y. Clears the
/// pending-motion latch and returns to scripted animation state 1 without
/// snapping position or resetting movementMode/movementSign. Child slots tick
/// on every call, including arrival; scratch is released before return.
void playerActorTickScriptedMoveTo(Task* task);

/// Resume choices for `playerActorEndScripted`.
enum {
    PLAYER_ACTOR_END_SCRIPTED_DEFAULT          = 0,
    PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION  = 1,
    PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET = 2,
};

/// Ends scripted control and restores the equipped weapon's native animation bank.
///
/// Returns 1 without changes unless the actor is in scripted mode; otherwise
/// returns 0. Restores normal playback rate, requests collision enablement and
/// enables view triggers. Choice 2 preserves model part 1's horizontal offset;
/// other choices rotate it into the root translation and clear its X/Z.
/// In battle, rebuilds playback and resumes aim locomotion for choice 2 or aim
/// entry otherwise. Outside battle, choice 2 starts aim exit, choice 1 rebuilds
/// playback and resets locomotion, and other values blend to locomotion over
/// four whole normal-rate frames.
/// Requires live actor/model, native bank and pose resources, equipped-weapon
/// and save state. Positions use the root-parent frame; no pointer is retained.
/// The message ID and second argument are unused.
s32 playerActorEndScripted(Task* task, s32 unusedMessageId, s32 resumeMode, s32 unusedSecondArg);

/// Replaces the player's queued XYZ displacement for the next movement update.
///
/// Requires a live player task. Borrows exactly three readable s32 components
/// in the player's root-parent coordinate frame, in whole coordinate units.
/// The movement update adds this displacement to the root translation and
/// clears it; repeated calls before that update replace the earlier request.
/// No pointer is retained.
void playerActorSetPendingDisplacement(const VECTOR3* displacement);

void Gp_PlayerWorkTask(Task* arg0);

/// Returns the XZ distance between two points using `SquareRoot0`, ignoring Y.
///
/// Borrows two readable, word-aligned three-component points in one coordinate
/// frame, in signed game-coordinate units. Stages all XYZ differences, then
/// square-roots the sum of the squared absolute X/Z differences. Differences,
/// absolute values, squares and their sum must fit s32. Reads exactly 12 bytes
/// from each point; no fourth vector word is required. Needs a live scratch
/// stack with 16 free bytes, released before return; no pointer is retained.
s32 playerActorPlanarDistance(const VECTOR3* firstPoint, const VECTOR3* secondPoint);

/// Emits a new surface footstep cue and returns its adjusted sound-event ID.
///
/// Processes each non-NULL slot-1 record identity once, retaining that identity
/// in `lastCueRecord`. Cue 1 selects base+1; cue 2 selects base. Scripted stair
/// climbing selects the stair entry, running selects the run entry and latches
/// the footstep action signal, otherwise the walk entry is used. Companions add
/// 100 to a non-silent event. Returns zero for no new audible cue, and returns
/// the chosen ID even if sound enqueueing fails. A surface-sound record also
/// permits optional room dust at model parts 15/18 (player) or 16/19 (companion).
/// Requires live actor playback/model coordinates, initialized current-stage,
/// area and surface tables, sound/effect state and loaded cue records. Model
/// parts used by sound and dust must exist. Sound and effect allocation results
/// do not change the consumed-record guard.
s32 playerActorPlayFootstepCue(Task* task);

void Gp_PlayerMode2State0(Task* arg0);

void Gp_PlayerMode2State1(Task* arg0);

/// Advances the player or companion's scripted yaw turn, mode 2 state 2.
///
/// Angles use 4096 units per turn. Snaps within 64 units of the stored target
/// or its one-turn lower image; otherwise steps the shortest turn by at most
/// 64 units and wraps to 0..4095. Completion clears the motion latch, enters
/// scripted idle state 1 and blends to native clip 1 over five normal-rate
/// frames. Child slots tick on every call. Requires live actor/native playback
/// and scratch resources; does not move the root or take ownership.
void playerActorMode2State2(Task* task);

/// Counts newly pressed direction/face-button ticks until a scripted hold completes.
///
/// `actionValue` counts ticks with any matching press, against `stateTimer`;
/// multiple simultaneous presses count once. Completion is checked before the
/// tick's press and broadcasts `ACTOR_MESSAGE_RELEASE_HOLD` once. While complete,
/// recovery is reset to 18 ticks on each call. Child animation slots keep ticking.
/// Requires live actor work and animation resources for `playerActorTickChildSlots`.
void playerActorMode2State6(Task* task);

/// Model draw modes accepted by `playerActorSetModelDraw`.
enum {
    PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE = 0,
    PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO     = 1,
    PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE  = 2,
    PLAYER_ACTOR_MODEL_DRAW_HIDE_KEEP     = 3,
    PLAYER_ACTOR_MODEL_DRAW_SHOW_ALLOCATE = 4,
};

/// Sets actor-model draw/buffer flags and propagates the complete flags to attachments.
///
/// Modes 0/4 allocate the parent buffer, 2 releases it, and 1/3 leave it alone.
/// Modes 0/2/3 skip active drawing; 2/3 also skip automatic buffer allocation.
/// Other modes retain the parent's flags and still propagate them. Returns 0,
/// discarding allocation status. Each attachment and equipped-model child repeats
/// the selected operation on the parent model; the equipped model itself only
/// receives flags. All present tasks/models and the circular child list must be
/// live. Buffer heap, stream and GPU-lifetime requirements follow the TMD APIs.
s32 playerActorSetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg);

/// Places an actor's model root from a borrowed position and XYZ Euler rotation.
///
/// Copies XYZ position in the root's parent frame, in game-coordinate units,
/// and angles in 4096 units per turn. Builds and normalizes the local rotation,
/// invalidates the root cache and composes it through its existing parent chain.
/// Task, `GameActor` work and model coordinates must be live; composition follows
/// `actorRenderComposeCoord` requirements. Reads no vector fourth components,
/// retains no transform pointer and returns 0. Stored previous position is untouched.
s32 playerActorPlace(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg);
/// Takes scripted control and starts a straight walk counted by sounded footsteps.
///
/// Resets movement, aim offsets and attack effects, selects scripted state 5
/// and disables collision. Borrows the request only during this call, narrowing
/// the count to s16 and retaining the complete second s32 word. Its role is unproven.
/// The player counts audible footstep cues; companions using this handler have
/// no walk state and remain pending until scripted control ends. Requires live
/// actor work, weapon effects and session state; later walking needs native
/// animation/model resources. Returns 0; message ID and second argument unused.
s32 playerActorWalkSteps(Task* task, s32 unusedMessageId, const GameActorWalkSteps* walkSteps, s32 unusedSecondArg);
/// Returns the live actor's signed scripted-motion latch (0 complete, 1 pending).
///
/// No payload is used and no state is changed; the task must have `GameActor` work.
s32 playerActorIsScriptedMotionPending(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Returns 1 while any child animation slot has not settled, otherwise 0.
///
/// Reads slots `animationSlotCount - 1` down to 1; slot 0 is excluded and a count
/// at most 1 returns 0. The task's live actor must have an initialized active
/// prefix within `animationSlots`. No payload is used and playback is unchanged.
s32 playerActorIsAnimationPlaying(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Sets child-slot playback and future actor playback to a positive clamped rate.
///
/// `rate` is animation time in sixteenths of a normal frame per tick (16 normal),
/// clamped to 1..127 before byte storage. Updates slots 1 through
/// `animationSlotCount - 1` and `animationRate`, leaving slot 0 intact. The live
/// actor's active slot prefix must be in bounds. Returns 0; the second payload is unused.
s32 playerActorSetAnimationRate(Task* task, s32 unusedMessageId, s32 rate, s32 unusedSecondArg);

/// Reparents the actor's model root while preserving its composed transform.
///
/// `parent` and the task's model root must be live with acyclic parent chains.
/// The borrowed parent must remain live while attached and must not be the root
/// or its descendant. An unchanged parent is a no-op. Otherwise composes both
/// nodes, rewrites the root's local matrix in the new parent frame and marks it
/// dirty for later composition. Allocates nothing and returns 0.
s32 playerActorAttachToCoord(Task* task, s32 unusedMessageId, GfxCoord* parent, s32 unusedSecondArg);

/// Restarts one or both of the actor's texture-upload sequences.
///
/// Requests 0..5 cover the loaded player/companion tables: 0 selects A=1/B=2,
/// 1..3 select A=2..4, and 4..5 select B=1..2. Selected delay/frame bytes are
/// cleared; the other sequence is retained. The live actor's texture resources
/// must remain loaded for subsequent updates. No validation is performed;
/// selector stores keep their low byte, later read as signed. Returns 0.
s32 playerActorSetTextureSequence(Task* task, s32 unusedMessageId, s32 sequence, s32 unusedSecondArg);

#endif // GAMEPLAY_PLAYER_ACTOR_H
