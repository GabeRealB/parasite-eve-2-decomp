#include "rooms/neo_ark_altar.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "neo_ark_altar_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// The room's own `TaskMessageEntry[]` - the message table this task publishes.
extern TaskMessageEntry D_neo_ark_altar_8017EF98[];

/// Single-entry spawn table for the altar's cutscene-driver task
/// (`func_neo_ark_altar_8017D668`).
extern TaskDesc D_neo_ark_altar_8017EF8C;

static void func_neo_ark_altar_8017D974(Task* task);
static void func_neo_ark_altar_8017D9E0(Task* task);

/// State table of the room's message task: set-up
/// (`func_neo_ark_altar_8017D974`), an empty per-frame state and `taskKill`.
/// Its bytes open the room's rodata, ahead of the cutscene driver's jump table.
static const TaskFuncTable3 D_neo_ark_altar_8017D5C4 = {
    func_neo_ark_altar_8017D974,
    func_neo_ark_altar_8017D9E0,
    taskKill,
};

void func_neo_ark_altar_8017D668(Task*);
s32  func_neo_ark_altar_8017D8BC(Task*, s32, s32, s32);
s32  func_neo_ark_altar_8017D8C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_altar_8017D908(Task*, s32, s32, s32);
s32  func_neo_ark_altar_8017D910(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskDesc D_neo_ark_altar_8017EF8C = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_altar_8017D668, { .value = 0 } };

TaskMessageEntry D_neo_ark_altar_8017EF98[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_altar_8017D8C4 },
    { 5105, func_neo_ark_altar_8017D8BC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_altar_8017D910 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_altar_8017D908 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Altar cutscene driver: silences the player's weapon, runs cap command 2,
/// then branches on the cap event key to record the altar choice in game flag
/// 0xD9 before spawning the follow-up task and restoring control.
void func_neo_ark_altar_8017D668(Task* task)
{
    switch (task->state) {
        case 0:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
            gGameSession->hideHud                                      = 1;
            gGameSession->eventState                                   = 1;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            Gp_RunCapCmd(2, 0);
            task->state++;
            break;
        case 3:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 4:
            switch (capGetVariantKey()) {
                case 11:
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 0);
                    task->state++;
                    break;
                case 21:
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 1);
                    task->state++;
                    break;
                case 12:
                    task->state = 0xA;
                    break;
            }
            break;
        case 5:
            if ((gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_0F9) == 0) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0)) {
                gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_0F9, 1);
                Gp_ApplyAreaRecs(D_neo_ark_altar_801800A0);
            }
            sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SWITCH_TOGGLE, 0, 0);
            task->killCountdown = 0x1E;
            task->state++;
            break;
        case 6:
            func_neo_ark_altar_8017DC40(gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) & 0xFF);
            task->killCountdown--;
            if (task->killCountdown <= 0) {
                task->state++;
            }
            break;
        case 7:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) != 0) {
                taskSpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 0, 0);
            } else {
                taskSpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 1, 0);
            }
            task->state++;
            break;
        case 8:
            task->state = 0xA;
            break;
        case 10:
            SetDispMask(1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
            gGameSession->hideHud                                      = 0;
            gGameSession->eventState                                   = 0;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            taskKill(task);
            break;
    }
}

s32 func_neo_ark_altar_8017D8BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `RoomEventMsg` onto the outgoing one and passes both on to `mapNeoArkResolveRoomVariant`.
/// Always returns 1.
s32 func_neo_ark_altar_8017D8C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    return 1;
}

s32 func_neo_ark_altar_8017D908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `Gp_PostMsg13EF` posts. When the record's `field_2` is 1 and
/// agrees with the current view (`GameSession.location.loc.room`) the altar runs CAP
/// command 3; on any other view the same byte starts the overlay's
/// cutscene-driver task through `D_neo_ark_altar_8017EF8C`. Any other byte is
/// ignored, and the outgoing record is never written - this handler only
/// consumes the message.
s32 func_neo_ark_altar_8017D910(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->warp == 1) {
        if (gGameSession->location.loc.room == in->warp) {
            Gp_RunCapCmd1(3);
        } else {
            taskSpawnFromTable(&D_neo_ark_altar_8017EF8C, 0, 0, 0);
        }
    }
    return 0;
}

/// State 0 of the altar's message task: park the room's message table in
/// `Task::msgTable`, publish the task in pointer slot 7, bring the altar's
/// switch sprites one step towards the choice recorded in game flag 0xD9
/// (`func_neo_ark_altar_8017DC40`), then start the altar task from
/// `D_neo_ark_altar_8017F088` and advance to state 1.
static void func_neo_ark_altar_8017D974(Task* task)
{
    task->msgTable = D_neo_ark_altar_8017EF98;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    func_neo_ark_altar_8017DC40(gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) & 0xFF);
    taskSpawnFromTable(D_neo_ark_altar_8017F088, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room's message task: nothing to do, the task only
/// holds the message table.
static void func_neo_ark_altar_8017D9E0(Task* task)
{
}

/// Runs the room's message task's current state through a stack copy of
/// `D_neo_ark_altar_8017D5C4`.
void func_neo_ark_altar_8017D9E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_altar_8017D5C4;
    sp.funcs[task->state](task);
}
