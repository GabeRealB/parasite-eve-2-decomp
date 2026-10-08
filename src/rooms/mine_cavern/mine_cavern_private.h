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

/// Colours of the cavern's darkness, one per count of destroyed targets (0-4).
///
/// The colour is what a full-screen quad subtracts from the picture, so a
/// larger channel is a darker cavern. The rows run from an even grey with all
/// four targets intact to a weaker wash that leaves red untouched once all are
/// destroyed. Only the three colour channels are used; `cd` is zero.
extern CVECTOR D_mine_cavern_8018E3E0[5];

/// How many of the two steps of game flag nibble 0xE6 (values 1 and 2)
/// `mineCavernReleaseEventBattleHold` has already acted on; `mineCavernResetEventBattleReleaseCount`
/// resets it.
extern s32 D_mine_cavern_8018EB54;

extern u16 D_mine_cavern_8018E360;

extern u16 D_mine_cavern_8018E362;

extern u16 D_mine_cavern_8018E364;

extern u16 D_mine_cavern_8018E366;

extern u16 D_mine_cavern_8018E368;

extern SVECTOR D_mine_cavern_8018E36C[6];

extern SVECTOR D_mine_cavern_8018E39C[4];

extern u8 D_mine_cavern_8018E3BC[4][8];

extern s16 D_mine_cavern_8018E3DC;

extern EnemyParams D_mine_cavern_8018EAE4;

extern u8 D_mine_cavern_8018EAF4[36];

extern SVECTOR D_mine_cavern_8018EB18[4];

extern TaskDesc D_mine_cavern_8018EB38[2];

extern s32 D_mine_cavern_8018EB58;

extern u16 D_mine_cavern_8018EB5C;

extern TaskDesc D_mine_cavern_80183CA4[2];

extern EvsCommand D_mine_cavern_80187C74[41];

extern EvsCommand D_mine_cavern_8018804C[19];

extern EvsCommand D_mine_cavern_80188214[60];

extern EvsCommand D_mine_cavern_801887B4[27];

extern EvsCommand D_mine_cavern_80188A3C[31];

extern EvsCommand D_mine_cavern_80188D24[24];

extern AreaApplyRec D_mine_cavern_8018E32C[9];

extern TaskMessageEntry D_mine_cavern_80183C6C[7];

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

void func_mine_cavern_8017E088(s16);

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
s32 func_mine_cavern_8017D908(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_mine_cavern_8017DAA0(Task*, s32, s32, s32);

/// Refuses every key-item use at the cavern's room task.
///
/// All arguments are ignored and the reply is `ROOM_KEY_ITEM_USE_REFUSED`.
s32 mineCavernRefuseKeyItem(Task* task, s32 msgId, s32 itemId, s32 unusedArg);

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3);

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
