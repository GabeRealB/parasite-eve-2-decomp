#include "rooms/acropolis_fountain.h"

#include "types.h"

#include "acropolis_fountain_private.h"

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

static void func_acropolis_fountain_8017D960(Task* arg0);
static void func_acropolis_fountain_8017D9BC(Task* task);

/// State handlers of the room task: set-up, an empty per-frame tick and
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_fountain_8017D5C4 = {
    { func_acropolis_fountain_8017D960, func_acropolis_fountain_8017D9BC, taskKill },
};

s32  func_acropolis_fountain_8017D604(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_acropolis_fountain_8017D774(Task*, s32, s32, s32);
s32  func_acropolis_fountain_8017D77C(Task*, s32, s32, s32);
s32  func_acropolis_fountain_8017D7F4(Task*, s32, s32, s32);
void func_acropolis_fountain_8017D868(Task*);

TaskMessageEntry D_acropolis_fountain_8017E764[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_fountain_8017D604 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_fountain_8017D77C },
    { 5105, func_acropolis_fountain_8017D774 },
    { ROOM_MESSAGE_SOUND, func_acropolis_fountain_8017D7F4 },
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

/// Handler for message 0x13F1 in the room task's message table: ignores the
/// message and answers 0.
s32 func_acropolis_fountain_8017D774(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_acropolis_fountain_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 args[2];

    if (arg2 == 3) {
        capRunCommandWithTransition(((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2) == 0) ? 3 : 6);
    }
    if (arg2 == 4) {
        capStartSequenceSlot(4, 1, 0);
        func_acropolis_fountain_8017DA1C();
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_FOUNTAIN_012, 1);
    }
    return 0;
}

s32 func_acropolis_fountain_8017D7F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 3), 0, 0);
            break;
        case 4:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 4), 0, 0);
            break;
        case 9:
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
            taskSpawn(0, 0x11, 0, 0);
            gameFlagSetNibble(0, 5);
            taskKill(task);
            break;
    }
}

static void func_acropolis_fountain_8017D960(Task* arg0)
{
    arg0->msgTable = D_acropolis_fountain_8017E764;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_FOUNTAIN_012) != 0) {
        func_acropolis_fountain_8017DA1C();
    }
    arg0->state = (s32)(arg0->state + 1);
}

static void func_acropolis_fountain_8017D9BC(Task* task)
{
}

/// Runs the room task's current state through a stack copy of its three-entry
/// state table.
void func_acropolis_fountain_8017D9C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fountain_8017D5C4;
    sp.funcs[task->state](task);
}
