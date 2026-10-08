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

s32        func_acropolis_fountain_8017D604(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _acropolisFountainRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_acropolis_fountain_8017D77C(Task*, s32, s32, s32);
static s32 _acropolisFountainHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);
void       func_acropolis_fountain_8017D868(Task*);

TaskMessageEntry D_acropolis_fountain_8017E764[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_fountain_8017D604 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_fountain_8017D77C },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisFountainRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisFountainHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_fountain_8017E78C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_fountain_8017D868, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Message gate for the fountain's hotspot: copies the incoming record to the
/// outgoing one, then edits the copy's `room` (the answer the caller acts
/// on) according to the message id and the room's progress nibbles. Message 3
/// before nibble 0 reaches 5 hands the record's first two bytes to
/// `D_acropolis_fountain_80183BB0`/`BB1` and spawns the room's own task,
/// consuming the message (returns 0); from nibble 0 == 5 on it only answers.
s32 func_acropolis_fountain_8017D604(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;

    *out  = *in;
    msgId = in->areaId;
    if (msgId == 3) {
        if (gameFlagGetNibble(0) < 5) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                D_acropolis_fountain_80183BB0 = in->warp;
                D_acropolis_fountain_80183BB1 = in->room;
                taskSpawnFromTable(D_acropolis_fountain_8017E78C, 0, 0, 0);
            }
            return 0;
        }
        if (in->areaId == msgId && in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(0) < 2) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < 2) {
                    out->room = 1;
                } else {
                    out->room = 2;
                }
            } else {
                out->room = msgId;
            }
        }
    } else if (msgId == 9) {
        if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 1) {
            out->room = 2;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED) == 0) {
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

s32 func_acropolis_fountain_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 args[2];

    if (arg2 == 3) {
        capRunCommandWithTransition(((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2) == 0) ? 3 : 6);
    }
    if (arg2 == 4) {
        capStartSequenceSlot(4, 1, 0);
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

void func_acropolis_fountain_8017D868(Task* task)
{
    switch (task->state) {
        case 0:
            capRunCommandWithTransition(1);
            task->state = task->state + 1;
            break;

        case 1:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            /* fallthrough */

        case 2:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_ACROPOLIS_PATIO;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_acropolis_fountain_80183BB0;
            gDisplayState.spriteVariant                                = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            gameFlagSetNibble(0, 5);
            taskKill(task);
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
