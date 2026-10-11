#include "gameplay/captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/evs.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"

/* Define BSS before API headers to preserve first-declaration order. */
/// Scene-sync timer latch (0 disarmed, 1 armed); inactive CAP playback pauses it.
static s8 _gCapSceneSyncArmed;

/// Whether the last scene-selection message supplied a key (0 no, 1 yes).
///
/// Selects the subsequent scene-sync timing branch. A NULL selection clears
/// it without changing the CD descriptor; control-task initialization does
/// not reset it.
static s8 _gCapHasSceneKey;

/// Word set to one on every scene-selection message; its purpose is unproven.
///
/// No source reader or clearing store is identified. Retained with external
/// linkage because the optional external debug routines have no source here.
s32 D_801156B4;

/// Held demo-scene HUD slide task, or NULL when CAP holds no slide.
///
/// Hide retains the spawned task and suppresses duplicate hides. Show requests
/// its return and releases this handle before the task finishes; abort kills a
/// held task. Control-task initialization clears the handle without killing it.
static Task* _gCapHudSlideTask;

s16 gCapSceneSyncElapsedTicks;

#include "captions.h"

extern TaskMessageEntry D_8010FB90[10];

static s32 _capStartControlledSequence(Task* unusedTask, s32 unusedMessageId, s16 commandIndex, s32 unusedSecondArg);

static s32 _capResumeTimedRecord(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capAbortControlledPlayback(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capQueryPlaybackBusy(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capHideEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capReleaseEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capAbortEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _capSelectScene(Task* unusedTask, s32 unusedMessageId, const EvsSceneKey* sceneKey, s32 unusedSecondArg);

static s32 _capBeginSceneSync(Task* unusedTask, s32 unusedMessageId, s32 viewChangePhase, s32 unusedSecondArg);

void func_80724120(void);

void func_80724324(void);

enum {
    CAP_CONTROL_SCENE_SYNC_DISABLED  = 0,
    CAP_CONTROL_SCENE_SYNC_ARMED     = 1,
    CAP_CONTROL_HUD_SLIDE_DEMO_SCENE = 9,
    CAP_CONTROL_HUD_SLIDE_RETURN     = 1
};

TaskMessageEntry D_8010FB90[10] = {
    { CAP_CONTROL_MESSAGE_START, _capStartControlledSequence },
    { CAP_CONTROL_MESSAGE_RESUME_TIMED_RECORD, _capResumeTimedRecord },
    { CAP_CONTROL_MESSAGE_ABORT, _capAbortControlledPlayback },
    { CAP_CONTROL_MESSAGE_IS_BUSY, _capQueryPlaybackBusy },
    { CAP_CONTROL_MESSAGE_HIDE_HUD, _capHideEventHud },
    { CAP_CONTROL_MESSAGE_SHOW_HUD, _capReleaseEventHud },
    { CAP_CONTROL_MESSAGE_SHOW_HUD_ABORT, _capAbortEventHud },
    { CAP_CONTROL_MESSAGE_SELECT_SCENE, _capSelectScene },
    { CAP_CONTROL_MESSAGE_BEGIN_SCENE_SYNC, _capBeginSceneSync },
    { -1, NULL },
};

/// Advances the armed scene-sync timer while CAP playback is selected.
///
/// Publishes completion when the elapsed count reaches thirty active ticks,
/// including a count primed by the request. Inactive playback pauses the count.
/// Completion disarms the timer; playback consumes and clears the bit. If the
/// bit was already set when armed, counting continues until playback clears it.
static inline void _capTickSceneSync(void)
{
    if (capIsBusy() != 0 && _gCapSceneSyncArmed != 0) {
        gCapSceneSyncElapsedTicks++;
        if ((gCapControlFlags & CAP_CONTROL_SCENE_SYNC_COMPLETE) == 0) {
            if (gCapSceneSyncElapsedTicks >= CAP_CONTROL_SCENE_SYNC_DELAY_FRAMES) {
                gCapControlFlags   |= CAP_CONTROL_SCENE_SYNC_COMPLETE;
                _gCapSceneSyncArmed = CAP_CONTROL_SCENE_SYNC_DISABLED;
            }
        }
    }
}

void capInitializeControlTask(Task* task)
{
    // No CAP control handler reads this allocation; preserve its four-byte extent.
    enum { CAP_CONTROL_WORK_BYTES = 4 };
    void* workStorage;

    workStorage = memCalloc(CAP_CONTROL_WORK_BYTES, false);
    if (workStorage == NULL) {
        taskKill(task);
        return;
    }
    capReset();
    _gCapHudSlideTask = NULL;
    task->msgTable    = D_8010FB90;
    gameSetTaskSlot(task, GAME_TASK_SLOT_CAP_CONTROL);
    task->work          = workStorage;
    _gCapSceneSyncArmed = CAP_CONTROL_SCENE_SYNC_DISABLED;
    task->state++;
}

void capUpdateControlTask(Task* unusedTask)
{
    if (gDisplayState.debugMode != 0) {
        func_80724120();
        func_80724324();
    }
    if (gCapFile != NULL) {
        capRelocateFile(gCapFile);
    }
    _capTickSceneSync();
}

/// Starts a CAP command's variant zero in place and disarms scene synchronization.
///
/// The first payload narrows to a signed halfword command-table slot and must
/// satisfy `capStartSequenceSlot`'s bounds and loaded-resource lifetime.
/// The receiver, message ID and second payload are ignored. Disarms the timer
/// even when playback is busy or cannot start; always returns 0.
static s32 _capStartControlledSequence(Task* unusedTask, s32 unusedMessageId, s16 commandIndex, s32 unusedSecondArg)
{
    enum { CAP_CONTROL_DEFAULT_VARIANT = 0 };

    capStartSequenceSlot(commandIndex, CAP_PLAYBACK_IN_PLACE, CAP_CONTROL_DEFAULT_VARIANT);
    _gCapSceneSyncArmed = CAP_CONTROL_SCENE_SYNC_DISABLED;
    return 0;
}

/// Releases the current timed CAP record's display and pause waits.
///
/// Clears both frame counters, including the indefinite-pause sentinel; record
/// advancement occurs in the playback task. Both payloads are ignored. Returns 0.
static s32 _capResumeTimedRecord(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    gCapPauseFramesRemaining   = 0;
    gCapDisplayFramesRemaining = 0;
    return 0;
}

/// Disarms scene synchronization and aborts allocated CAP playback.
///
/// Both payloads are ignored. Returns `capAbortPlayback`'s result (0 after
/// cleanup, -1 without a selected sequence or allocated task). A queued sequence
/// with no task remains selected.
static s32 _capAbortControlledPlayback(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _gCapSceneSyncArmed = CAP_CONTROL_SCENE_SYNC_DISABLED;
    return capAbortPlayback();
}

/// Returns whether CAP has a selected sequence, including queued playback.
///
/// Both payloads are ignored; the result is 1 for selected playback, otherwise 0.
static s32 _capQueryPlaybackBusy(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return capIsBusy();
}

/// Suppresses the event HUD, sliding the HP/MP display upward in saved demo scene 9.
///
/// The demo path retains one slide task until a show or abort request releases
/// its handle. Repeated hides leave that task and its direction unchanged.
/// Allocation failure leaves the handle NULL, so a later hide can retry.
/// Other scenes set the session's HUD suppression flag. The receiver, message
/// ID and both payloads are ignored. Always returns 0, including spawn failure.
static s32 _capHideEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum {
        CAP_CONTROL_HUD_SLIDE_TASK_BANK  = 9,
        CAP_CONTROL_HUD_SLIDE_TASK_INDEX = 8
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == CAP_CONTROL_HUD_SLIDE_DEMO_SCENE) {
        if (_gCapHudSlideTask != NULL) {
            return 0;
        }
        _gCapHudSlideTask = taskSpawn(CAP_CONTROL_HUD_SLIDE_TASK_BANK, CAP_CONTROL_HUD_SLIDE_TASK_INDEX, 0, 0);
    } else {
        gGameSession->hideHud = true;
    }
    return 0;
}

/// Releases event HUD suppression, returning the demo HUD by its slide animation.
///
/// Demo scene 9 hands the live slide task back to its own teardown by reversing
/// its direction and releasing the held handle. The slide must have initialized;
/// initialization overwrites an earlier direction request. Other scenes clear
/// the HUD suppression flag. Both payloads are ignored. Returns 0.
static s32 _capReleaseEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    Task* hudSlideTask;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == CAP_CONTROL_HUD_SLIDE_DEMO_SCENE) {
        hudSlideTask = _gCapHudSlideTask;
        if (hudSlideTask != NULL) {
            hudSlideTask->spawnArg1.value = CAP_CONTROL_HUD_SLIDE_RETURN;
            _gCapHudSlideTask             = NULL;
            return 0;
        }
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

/// Releases normal event HUD suppression or aborts the demo HUD slide immediately.
///
/// Demo scene 9 kills its held slide task without resetting the current HUD
/// offset. A missing handle does nothing. Other scenes clear the HUD suppression
/// flag. Both payloads are ignored. Returns 0.
static s32 _capAbortEventHud(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == CAP_CONTROL_HUD_SLIDE_DEMO_SCENE) {
        if (_gCapHudSlideTask == NULL) {
            return 0;
        }
        taskKill(_gCapHudSlideTask);
        _gCapHudSlideTask = NULL;
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

/// Selects the event's scene/audio descriptor and records whether a key was supplied.
///
/// `sceneKey` borrows a halfword-aligned six-byte key for this dispatch only.
/// NULL leaves the CD selection intact and records that no key was supplied.
/// Selection and descriptor lifetime follow `cdCmdSelectScene`; buffer setup and
/// playback happen separately. The second payload is ignored. Returns 0.
static s32 _capSelectScene(Task* unusedTask, s32 unusedMessageId, const EvsSceneKey* sceneKey, s32 unusedSecondArg)
{
    if (sceneKey != NULL) {
        cdCmdSelectScene(sceneKey->group, sceneKey->streamId, sceneKey->subId);
    }
    D_801156B4       = 1;
    _gCapHasSceneKey = sceneKey != NULL;
    return 0;
}

/// Arms the CAP scene-sync gate unless the event has been skipped.
///
/// A supplied scene key always starts the thirty-tick wait and calls the resident
/// scene-control no-op. Without a key, `viewChangePhase == 2` (view change with
/// scene synchronization) primes the timer so the next active control tick
/// completes it; every other value starts at zero. The second payload is ignored.
/// Returns 0, including skipped events, whose existing state is retained.
static s32 _capBeginSceneSync(Task* unusedTask, s32 unusedMessageId, s32 viewChangePhase, s32 unusedSecondArg)
{
    enum { CAP_CONTROL_VIEW_CHANGE_WITH_SCENE_SYNC = 2 };

    if (gGameSession->evtSkipped == 0) {
        if (_gCapHasSceneKey != 0) {
            cdCmdSceneControlNoOp();
            _gCapSceneSyncArmed       = CAP_CONTROL_SCENE_SYNC_ARMED;
            gCapSceneSyncElapsedTicks = 0;
        } else {
            _gCapSceneSyncArmed = CAP_CONTROL_SCENE_SYNC_ARMED;
            if (viewChangePhase == CAP_CONTROL_VIEW_CHANGE_WITH_SCENE_SYNC) {
                gCapSceneSyncElapsedTicks = CAP_CONTROL_SCENE_SYNC_DELAY_FRAMES;
            } else {
                gCapSceneSyncElapsedTicks = 0;
            }
        }
    }
    return 0;
}

void capControlTask(Task* task)
{
    TaskFuncTable3 states;

    states = gCapControlStates;
    states.funcs[task->state](task);
}
