#ifndef SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H
#define SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"

#include "main/task_types.h"

// Target spots share an index across placements, effects and saved destruction bits.
enum {
    MINE_CAVERN_TARGET_COUNT                = 4,
    MINE_CAVERN_TARGET_EFFECT_VIEW_CAPACITY = 8,
    MINE_CAVERN_TARGET_DAMAGE_ROW_COUNT     = 36,
    /// Absent mapped view: ends a short visibility row or initializes the previous view.
    MINE_CAVERN_TARGET_EFFECT_VIEW_NONE = 0
};

/// Subtractive darkness RGB selected by the number of destroyed targets, 0..4.
///
/// Each byte channel is subtracted by a full-screen quad: larger values darken
/// more. `cd` is unused and zero. The four saved destruction bits bound the
/// count; there is no terminator. Keep the cavern loaded through drawing.
extern CVECTOR gMineCavernDarknessColors[MINE_CAVERN_TARGET_COUNT + 1];

/// Number of credited cavern event battle-hold releases (0 none, 1 first, 2 both).
///
/// Reset for the intro, then incremented once per persistent event stage 1/2.
/// A release is credited even without an active battle hold. Repeated callbacks
/// do not increment it; it is separate from the saved event-progress nibble.
extern s32 gMineCavernEventBattleReleaseCount;

/// Red intensity of a destroyed target's transient light, with 12 fractional bits.
///
/// Stored as an unsigned halfword and copied into the light's signed RGB channel;
/// the authored value 6016 is below 32768. Requires the cavern overlay loaded.
extern u16 gMineCavernTargetLightRed;

/// Green intensity of a destroyed target's transient light, with 12 fractional bits.
///
/// The unsigned halfword holds 2048 (half intensity) and is copied into the
/// light's signed RGB channel. Requires the cavern overlay loaded.
extern u16 gMineCavernTargetLightGreen;

/// Blue intensity of a destroyed target's transient light, with 12 fractional bits.
///
/// The unsigned halfword holds 2048 (half intensity) and is copied into the
/// light's signed RGB channel. Requires the cavern overlay loaded.
extern u16 gMineCavernTargetLightBlue;

/// Full-strength radius of a destroyed target's transient light, in world units.
///
/// The unsigned halfword supplies 950 units, inside the jittered outer radius.
/// Requires the cavern overlay loaded.
extern u16 gMineCavernTargetLightInnerRadius;

/// Base falloff radius of a destroyed target's transient light, in world units.
///
/// Each refresh adds 0..2047 units to the authored 2710-unit radius, giving
/// 2710..4757 units at which the light reaches zero. Requires the cavern loaded.
extern u16 gMineCavernTargetLightBaseOuterRadius;

/// Six authored fixed-glow centres in whole world-coordinate units.
///
/// Every XYZ entry is projected through the view; `pad` is unused and zero.
/// Entry 3 is suppressed in mapped view 4. This counted array has no terminator
/// and remains borrowed while the cavern's darkness controller draws.
extern SVECTOR gMineCavernFixedGlowPoints[6];

/// World centres of the four destroyed targets' glow, light, puff and sound effects.
///
/// Index 0..3 is the saved destruction-bit index. XYZ uses whole world units;
/// `pad` is unused and zero. These are independent of model-root placements in
/// `gMineCavernTargetModelPositions`. No terminator; keep the cavern loaded.
extern SVECTOR gMineCavernTargetEffectPoints[MINE_CAVERN_TARGET_COUNT];

/// Mapped views that show each destroyed target's glow and puff effects.
///
/// Rows use target index 0..3; each holds up to eight 1-based mapped views
/// in 1..25. Zero ends a short row; row 0 fills its capacity without a sentinel.
/// Light and positional-sound updates do not use this visibility filter.
/// Borrowed by the effects controller while the cavern overlay is loaded.
extern u8 gMineCavernTargetEffectViews[MINE_CAVERN_TARGET_COUNT][MINE_CAVERN_TARGET_EFFECT_VIEW_CAPACITY];

/// Mapped view sampled by the preceding target-effects update (0 before any update).
///
/// Later values are mapped views 1..25. A changed view bypasses the puff cadence
/// test; every update stores the view even while actors are paused or hidden.
/// Storage resets to its initialized zero when the cavern overlay is loaded.
extern s16 gMineCavernTargetEffectsPreviousView;

/// Intact target parameters: 30 starting HP, one attack row and zero release rewards.
///
/// Critical chance, buildup and damage-over-time settings are also zero. Each
/// intact enemy borrows this record and copies its starting HP; keep the cavern
/// loaded until the enemies are destroyed. Remains tasks do not install it.
extern EnemyParams gMineCavernTargetParams;

/// HP lost by an intact target for attack rows 0..35 of a non-attachment hit.
///
/// Indexed by the contact key's low seven bits after rejecting bit 15; callers
/// must keep that row below 36. The byte replaces the ordinary damage roll's
/// result, while the roll's side effects are retained. Rows 0 and 33..35 deal
/// zero. This is a cavern-specific prefix, not the full resident attack table.
/// Keep the cavern overlay loaded while its target tasks can take hits.
extern u8 gMineCavernTargetDamageByAttackRow[MINE_CAVERN_TARGET_DAMAGE_ROW_COUNT];

/// World placements shared by the intact and remains models at target spots 0..3.
///
/// XYZ uses whole world-coordinate units; `pad` is unused and zero. Spawning
/// copies XYZ into the model root parented to the view coordinate. The index
/// also selects the saved destruction bit and target-effects tables.
/// No terminator; the loaded cavern supplies all four entries.
extern SVECTOR gMineCavernTargetModelPositions[MINE_CAVERN_TARGET_COUNT];

/// Priority-96 TMD task recipes: index 0 intact target, index 1 remains model.
///
/// There is no terminator. The target controller spawns an intact task only
/// for an unset destruction bit and a remains task at every spot, passing
/// spot index 0..3 in `spawnArg1`. Models borrow cavern sources through teardown.
extern TaskDesc gMineCavernTargetTaskDescs[2];

/// Destruction nibble sampled by the preceding target-effects update, initially zero.
///
/// Mirrors `GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED` (bits 0..3 select spots).
/// A changed mask refreshes destroyed targets' positional sounds. Every update
/// replaces it, including paused/hidden updates; loading the cavern clears BSS.
extern s32 gMineCavernTargetEffectsDestroyedMask;

/// Running target-effects updates counted modulo 65536, initially zero on load.
///
/// Incremented once per controller update while actors run, even with no
/// destroyed target. Puff cadence tests the signed low halfword modulo nine;
/// view changes also allow a puff. Paused/hidden updates retain the counter.
extern u16 gMineCavernTargetEffectsTicks;

/// Bodyless task recipe that commits the final power-panel or passage caption choice.
///
/// Entry 0 spawns the priority-32 watcher; entry 1 is the descriptor terminator.
/// The watcher waits until CAP is idle, commits progress and releases itself.
/// Spawning borrows the recipe only synchronously; keep CAP and cavern code
/// loaded until the independent watcher finishes.
extern TaskDesc gMineCavernCaptionProgressTasks[2];

/// Cavern intro: player approach and CAP cues, actor reveal, then scripted battle.
///
/// All 41 instructions, including the final END, are borrowed through playback.
/// Requires the initial Mine/Shelter cavern layout, placed Blizzard Chaser slot 0,
/// and loaded player/weapon, CAP and room animation resources. Installs the
/// equipped-weapon scene extension before movement and releases player scripting
/// while preserving the animated root offset. Pair with `gMineCavernIntroSkipScript`.
extern EvsCommand gMineCavernIntroScript[41];

/// Fade-covered skip path that reaches the cavern intro's battle setup.
///
/// All 19 instructions, including END, are borrowed through playback. Places the
/// player at the approach destination, installs the scene extension and sets both
/// actors' battle poses before restoring view 3 and releasing player scripting.
/// Requires the same live actor, CAP and room resources as `gMineCavernIntroScript`.
extern EvsCommand gMineCavernIntroSkipScript[19];

/// First cavern event: burn and reset the Blizzard Chaser, then resume battle.
///
/// All 60 instructions, including END, are borrowed through playback. Places
/// the player and actor slot 0, installs the player's scene-bank extension,
/// runs CAP slot 2 and actor sound cues, then selects countdown music entry 4.
/// Requires live player/actor, CAP, weapon, sound and cavern scene resources.
/// Pair with `gMineCavernFirstEventSkipScript`.
extern EvsCommand gMineCavernFirstEventScript[60];

/// Fade-covered skip path to the first cavern event's resumed battle.
///
/// All 27 instructions, including END, are borrowed through playback. Sets
/// the final placements and actor commands, releases the previous battle hold,
/// starts CAP slot 2 variant 1 and prepares the new battle at view 6. Requires
/// live player/actor and CAP resources, with the cavern scene extension already
/// installed. Keep sound, weapon and cavern resources loaded through playback.
extern EvsCommand gMineCavernFirstEventSkipScript[27];

/// Second cavern event: smolder the Blizzard Chaser and open the post-battle layout.
///
/// All 31 instructions, including END, are borrowed through playback. Installs
/// player scene clips, runs CAP slot 3 at views 18/19, then activates room 2,
/// selects view 23, commits saved-area updates and starts the actor's dissolve.
/// Requires live player/actor slot 0, CAP and loaded weapon/cavern resources.
/// Pair with `gMineCavernSecondEventSkipScript`.
extern EvsCommand gMineCavernSecondEventScript[31];

/// Fade-covered skip path committing the second cavern event's final layout.
///
/// All 24 instructions, including END, are borrowed through playback. Sets
/// player/actor poses, starts smolder and dissolve, activates room 2 and dirty
/// view 23, releases the battle hold and applies the same saved-area updates.
/// Requires live player/actor slot 0 and loaded weapon/CAP/cavern resources.
extern EvsCommand gMineCavernSecondEventSkipScript[24];

/// Saved mine layouts and map marks committed after the cavern's second event.
///
/// Eight updates plus `AREA_APPLY_END`. Sets layouts 2/4 in six mine areas,
/// clearing the cavern mark and setting the others; Bounty/Nightmare also set
/// layout 11 and marks in the B1 storeroom and main corridor. Applying a layout
/// discards that area's saved enemy poses. Borrowed synchronously while the
/// Mine/Shelter saved-area directories remain loaded.
extern AreaApplyRec gMineCavernPostEventAreaUpdates[9];

/// Cavern room-task dispatch for transitions, key items, actions, commands, events and sound.
///
/// Six synchronous handlers followed by `TASK_MESSAGE_TABLE_END`. The live
/// room task borrows this table until teardown; payload types and results
/// follow each message's handler contract. Keep cavern code and data loaded.
extern TaskMessageEntry gMineCavernRoomMessages[7];

/// Resets the two-stage cavern event's credited battle-release count.
///
/// Call before a new cavern intro event; later stage releases increase the
/// count once per stage. This does not release holds or alter battle rewards.
void mineCavernResetEventBattleReleaseCount(void);

/// Hides or shows the cavern's sprite batches controlled by nursery progress.
///
/// Only the low byte of `hiddenValue` is used: 0 shows, 1 hides, and other
/// byte values leave visibility unchanged. Requires the cavern's stage/area
/// sprite directory and its mutable view/batch arrays to remain loaded.
void mineCavernSetProgressSpritesHidden(s32 hiddenValue);

// Callbacks referenced by the overlay's shared data tables.
/// Commits the final CAP choice to mine progress once caption playback is idle.
///
/// Key 11 completes the power-panel stage and clears its switched-on flag;
/// key 21 opens the secret passage and clears the cavern map mark. Other keys
/// leave progress intact. The live task is killed after any idle result.
void mineCavernCommitCaptionProgressTask(Task* task);

/// Releases one scripted battle hold per cavern event stage and credits actor 0's rewards.
///
/// Each first release requests weapon restoration and stores the low byte of
/// `endDelayFrames`; repeated calls only shorten the current delay. The comparison
/// uses the full signed argument before truncation, so normal callers use 0..255
/// ticks. Requires a live session and, when a battle hold exists, a live placed
/// actor 0 with enemy work. The release count must have been reset for this event.
void mineCavernReleaseEventBattleHold(s32 endDelayFrames);

/// Starts CAP variant 1 from a command slot with a queued display transition.
///
/// `commandIndex` is a nonnegative signed halfword within the live relocated
/// command table. Bypasses the command opcode; the table, caption and text
/// resources must stay loaded through playback. Busy playback or a missing
/// variant leaves it unchanged; the start result is discarded. Used by the
/// first event scene's skip path with command slot 2.
void mineCavernStartCaptionVariantOne(s16 commandIndex);

/// Engages the cavern's scripted battle, acquiring a hold only if none exists.
///
/// Resets the phase to idle before engaging, retaining rewards and other combat
/// state. The hold remains until a battle-release callback consumes it.
void mineCavernEngageScriptedBattle(void);

/// Controls automatic area-music changes during the cavern's event scripts.
///
/// Zero suppresses music changes and selects area-only music loading; nonzero
/// permits music changes while retaining the load-only flag. Requires a live session.
void mineCavernSetAreaMusicEnabled(s32 enabled);

/// Arms the live save's scene-music event for the resident music task.
///
/// Stores the signed-byte event selector without loading or starting music.
/// The cavern's first event scene selects event 11 on normal and skip paths.
void mineCavernSetSceneMusicEvent(s8 sceneEvent);

/// Applies the cavern event's persistent mine layouts and map marks.
///
/// Used after the second scene on normal and skip paths. The saved-area
/// directories for the mine must be loaded; the update list is borrowed synchronously.
void mineCavernApplyPostEventAreaUpdates(void);

/// Selects the countdown-music entry for subsequent scripted battle music.
///
/// Stores a byte selector without starting music; the event scripts use entries
/// 2 and 4 in the mine stage's 20-entry countdown table. Later music requests
/// require the stage's loaded map overlay and an in-range `countdownEntry`.
void mineCavernSelectCountdownMusicEntry(u8 countdownEntry);

/// Plays the cavern's two timed sound entries, then releases its task.
///
/// Requires a live task with `killCountdown` initialized to zero. That signed
/// halfword counts elapsed callback ticks; sounds run at twelve fixed ticks,
/// before the skip/expiry check. An event skip or tick 537 kills the task.
/// The loaded type-1 sound bank supplies entries 0x39 and 0x3A.
void mineCavernTimedSoundTask(Task* task);

/// Queues a fade-out of all MIDI sequences over 100 audio updates.
///
/// Queue admission failures are discarded; audio fade rounding can extend the duration.
void mineCavernFadeOutMusic(void);

/// Locks attachment selection and requests cancellation of all room effects.
///
/// The lock remains until gameplay clears it after the event. Cancellation is
/// processed by effect tasks rather than releasing their storage synchronously.
void mineCavernLockAttachmentsAndCancelEffects(void);

/// Runs target spawning, per-frame cavern effects and controller teardown.
///
/// Requires a live task with state 0..2, initially 0, and loaded cavern resources.
/// State 0 spawns intact/remains tasks; state 1 draws the darkness and effects;
/// state 2 releases this controller. Attract demo scene 3 suspends all dispatch.
/// The handler table is copied by value and the state is not bounds checked.
void mineCavernTargetEffectsTask(Task* task);

/// Dispatches one intact cavern target's spawn, hit, retirement and explosion states.
///
/// `task` is live with state 0..4 and spot index 0..3 in `spawnArg1`; its
/// `spawnArg2.pointer` borrows the enemy work. The model and cavern resources
/// must stay loaded. State 4 destroys the enemy after its bodies are retired;
/// the copied five-handler table is indexed without a bounds check.
void mineCavernTargetTask(Task* task);

/// Dispatches the target-remains model's spawn, visible tick and teardown.
///
/// `task` is live with state 0..2 and spot index 0..3 in `spawnArg1`; its
/// `spawnArg2.pointer` borrows enemy work. The model stays hidden until that
/// spot's destroyed bit is set. State 2 destroys the enemy; the copied
/// three-handler table is indexed without a bounds check. Cavern resources
/// and model coordinates must remain loaded until teardown.
void mineCavernTargetRemainsTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Resolves a departure from the cavern and returns 1 to allow it or 0 to block it.
///
/// Borrows a complete request and writable eight-byte reply through synchronous
/// dispatch; they may alias. Copies the request and resolves the destination
/// room before testing passage progress or a gorge departure during combat.
/// Queries make no persistent changes. Execute requests can start a blocking
/// caption, set the optional flag to 2, or commit the first open-passage entry
/// and reset its follow-up dialogue. Requires the Mine/Shelter map resolver,
/// live session and loaded CAP resources; neither payload is retained.
s32 mineCavernResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Selects the cavern's passage-status and target captions on a room command.
///
/// Command 1 is ignored after nursery progress begins; otherwise passage/panel
/// state and battle state choose its caption, sometimes spawning a final-choice
/// watcher. In layouts 1 and 4, commands 8/14/15/16 select target spots 0..3
/// and use that spot's destroyed bit as CAP variant 0/1. Other commands do
/// nothing. Requires live session and loaded cavern/CAP resources; returns
/// zero regardless of playback or task-allocation success. Other arguments
/// are ignored.
s32 mineCavernHandleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedArg);

/// Refuses every key-item use at the cavern's room task.
///
/// All arguments are ignored and the reply is `ROOM_KEY_ITEM_USE_REFUSED`.
s32 mineCavernRefuseKeyItem(Task* task, s32 msgId, s32 itemId, s32 unusedArg);

/// Starts caption command 6 for room action 6 while progress flag 0xC4 is 1.
///
/// Borrows a live `DirectionActionRequest` through dispatch; only its action
/// ID is read and no pointer is retained. Other actions do nothing. Requires
/// loaded CAP resources, ignores the other arguments and always returns zero.
/// The wider story role of flag 0xC4 is unproven.
s32 mineCavernHandleActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedArg);

/// Advances the cavern's two-stage event on an actor-event message and returns zero.
///
/// From stage 0, locks attachments, cancels room effects and arms the first
/// scene for the room tick. From stage 1, updates the objective and starts the
/// second scene with its skip path. Later stages are unchanged. Payloads are
/// ignored; requires a live session and the cavern's loaded event scripts.
s32 mineCavernAdvanceEvent(Task* task, s32 msgId, s32 unusedEventId, s32 unusedArg);

/// Handles cavern sound command 13 by starting entry 13 of its room sound bank.
///
/// Other commands are ignored. Returns zero regardless of sound queue admission;
/// the cavern's stage-4/area-2 sound bank must remain loaded through playback.
s32 mineCavernHandleSoundMessage(Task* task, s32 msgId, s32 soundCommand, s32 unusedArg);

#endif // SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H
