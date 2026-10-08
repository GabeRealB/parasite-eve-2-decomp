#include "rooms/acropolis_fountain.h"

#include "types.h"

#include "acropolis_fountain_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

extern TaskMessageEntry D_acropolis_fountain_8017E764[];
extern TaskDesc         D_acropolis_fountain_8017E78C[];

static void _acropolisFountainInitializeRoomTask(Task* task);
static void _acropolisFountainIdleRoomTask(Task* unusedTask);

/// State handlers of the room task: set-up, an empty per-frame tick and
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_fountain_8017D5C4 = {
    { _acropolisFountainInitializeRoomTask, _acropolisFountainIdleRoomTask, taskKill },
};

static s32  _acropolisFountainResolveTransitionMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisFountainRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisFountainHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _acropolisFountainHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);
static void _acropolisFountainPatioDepartureTask(Task* task);

TaskMessageEntry D_acropolis_fountain_8017E764[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisFountainResolveTransitionMessage },
    { ROOM_MESSAGE_COMMAND, _acropolisFountainHandleCommandMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisFountainRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisFountainHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_fountain_8017E78C[2] = {
    { { { TASK_BODY_NONE, 32 } }, _acropolisFountainPatioDepartureTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Resolves patio and forked-road departures from the fountain's story progress.
///
/// Borrows an eight-byte request and writable reply, which may alias. Copies
/// the request first. A patio departure before progress 5 returns 0 and, when
/// executing, latches the arrival selectors and starts the departure task.
/// Other departures return 1. Queries suppress task starts and flag writes;
/// the unlocked forked-road room is resolved even for queries. Receiver and
/// message ID are unused; the room resources must remain loaded.
static s32 _acropolisFountainResolveTransitionMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_FOUNTAIN_PATIO_DEPARTURE_COMPLETE = 5,
        ACROPOLIS_FOUNTAIN_PATIO_SCENE_COMPLETE     = 2,
        ACROPOLIS_FOUNTAIN_FIRST_LOCK_BIT           = 1,
        ACROPOLIS_FOUNTAIN_UNLOCKED_ROOM            = 2,
        ACROPOLIS_FOUNTAIN_TASK_PATIO_DEPARTURE     = 0
    };
    s32 destinationAreaId;

    *reply            = *request;
    destinationAreaId = request->areaId;
    if (destinationAreaId == GAME_AREA_ACROPOLIS_PATIO) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_FOUNTAIN_PATIO_DEPARTURE_COMPLETE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                D_acropolis_fountain_80183BB0 = request->warp;
                D_acropolis_fountain_80183BB1 = request->room;
                taskSpawnFromTable(D_acropolis_fountain_8017E78C, ACROPOLIS_FOUNTAIN_TASK_PATIO_DEPARTURE, 0, 0);
            }
            return 0;
        }
        if (request->areaId == destinationAreaId && request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_FOUNTAIN_PATIO_SCENE_COMPLETE) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < ACROPOLIS_FOUNTAIN_PATIO_SCENE_COMPLETE) {
                    reply->room = 1;
                } else {
                    reply->room = ACROPOLIS_FOUNTAIN_UNLOCKED_ROOM;
                }
            } else {
                reply->room = destinationAreaId;
            }
        }
    } else if (destinationAreaId == GAME_AREA_ACROPOLIS_FORKED_ROAD) {
        if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_FOUNTAIN_FIRST_LOCK_BIT) {
            reply->room = ACROPOLIS_FOUNTAIN_UNLOCKED_ROOM;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED) == 0) {
            gameFlagSetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED, 1);
        }
    }
    return 1;
}

/// Refuses key-item use without consuming the item or starting a room event.
///
/// All arguments are ignored. The reply selects the inventory's "No use now" notice.
static s32 _acropolisFountainRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Runs the fountain's lock-dependent CAP command or enables its saved climb trigger.
///
/// Command 3 selects CAP command 3 before the second lock is released, 6 after.
/// Command 4 starts CAP sequence 4, enables the climb trigger and saves that
/// change. Other commands do nothing. Remaining arguments are unused; returns 0.
static s32 _acropolisFountainHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        ACROPOLIS_FOUNTAIN_COMMAND_LOCK_CAP     = 3,
        ACROPOLIS_FOUNTAIN_COMMAND_ENABLE_CLIMB = 4,
        ACROPOLIS_FOUNTAIN_SECOND_LOCK_BIT      = 2,
        ACROPOLIS_FOUNTAIN_CAP_LOCKED           = 3,
        ACROPOLIS_FOUNTAIN_CAP_UNLOCKED         = 6,
        ACROPOLIS_FOUNTAIN_CLIMB_CAP_SEQUENCE   = 4
    };
    // Unused automatic storage retains this handler's original stack frame.
    s32 unusedStack[2];

    if (commandId == ACROPOLIS_FOUNTAIN_COMMAND_LOCK_CAP) {
        capRunCommandWithTransition(((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_FOUNTAIN_SECOND_LOCK_BIT) == 0) ? ACROPOLIS_FOUNTAIN_CAP_LOCKED : ACROPOLIS_FOUNTAIN_CAP_UNLOCKED);
    }
    if (commandId == ACROPOLIS_FOUNTAIN_COMMAND_ENABLE_CLIMB) {
        capStartSequenceSlot(ACROPOLIS_FOUNTAIN_CLIMB_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        acropolisFountainEnableClimbTrigger();
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_FOUNTAIN_012, 1);
    }
    return 0;
}

/// Queues the fountain sound script selected by a room sound cue.
///
/// Cues 3, 4 and 9 start the corresponding area-bank entry with its base mix.
/// Other cues do nothing. Receiver, ID and second payload are unused; always
/// returns zero, discarding sound-queue admission failures.
static s32 _acropolisFountainHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_3 = 3,
        ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_4 = 4,
        ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_9 = 9
    };

    switch (soundCue) {
        case ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_3:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 3), 0, 0);
            break;
        case ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_4:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 4), 0, 0);
            break;
        case ACROPOLIS_FOUNTAIN_SOUND_CUE_ENTRY_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 9), 0, 0);
            break;
    }
    return 0;
}

/// Commits the deferred patio destination and queues a reload before releasing the task.
static inline void _acropolisFountainReloadPatio(Task* task)
{
    enum {
        ACROPOLIS_FOUNTAIN_PATIO_ARRIVAL_ROOM       = 3,
        ACROPOLIS_FOUNTAIN_PATIO_SPRITE_VARIANT     = 1,
        ACROPOLIS_FOUNTAIN_PATIO_DEPARTURE_PROGRESS = 5
    };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_ACROPOLIS_PATIO;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACROPOLIS_FOUNTAIN_PATIO_ARRIVAL_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_acropolis_fountain_80183BB0;
    gDisplayState.spriteVariant                                = ACROPOLIS_FOUNTAIN_PATIO_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    gameFlagSetNibble(GAME_FLAG_ACROPOLIS_PROGRESS, ACROPOLIS_FOUNTAIN_PATIO_DEPARTURE_PROGRESS);
    taskKill(task);
}

/// Plays the fountain's departure CAP and reloads the patio at the latched warp.
///
/// Starts in state 0 with loaded CAP resources and an already latched warp.
/// The next update reloads patio room 3 even if CAP remains busy: state 1
/// falls through to departure. The requested room latch is not consumed.
/// Spawn arguments and work are unused; reload allocation failure still
/// advances story progress and releases this bodyless task.
static void _acropolisFountainPatioDepartureTask(Task* task)
{
    enum {
        ACROPOLIS_FOUNTAIN_PATIO_START_CAP   = 0,
        ACROPOLIS_FOUNTAIN_PATIO_CHECK_CAP   = 1,
        ACROPOLIS_FOUNTAIN_PATIO_RELOAD      = 2,
        ACROPOLIS_FOUNTAIN_PATIO_CAP_COMMAND = 1
    };

    switch (task->state) {
        case ACROPOLIS_FOUNTAIN_PATIO_START_CAP:
            capRunCommandWithTransition(ACROPOLIS_FOUNTAIN_PATIO_CAP_COMMAND);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FOUNTAIN_PATIO_CHECK_CAP:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            // Departure proceeds on this update regardless of CAP's busy result.
            /* fallthrough */

        case ACROPOLIS_FOUNTAIN_PATIO_RELOAD:
            _acropolisFountainReloadPatio(task);
            break;
    }
}

/// Registers the fountain's room-message receiver and restores its climb interaction.
///
/// Requires a live task in state 0 and initialized room triggers. Borrows the
/// message table, publishes the task in `GAME_TASK_SLOT_ROOM` and restores the
/// climb trigger when its saved progress flag is nonzero, then enters state 1.
static void _acropolisFountainInitializeRoomTask(Task* task)
{
    task->msgTable = D_acropolis_fountain_8017E764;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_FOUNTAIN_012) != 0) {
        acropolisFountainEnableClimbTrigger();
    }
    task->state = task->state + 1;
}

/// Keeps the fountain's room task idle after initialization, without changing it.
static void _acropolisFountainIdleRoomTask(Task* unusedTask)
{
}

void acropolisFountainRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_acropolis_fountain_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
