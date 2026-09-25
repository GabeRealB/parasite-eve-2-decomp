#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_altar.h"
#include "rooms/room_common.h"

/// `Mc_SaveData.at4.loc.view`, the area id forced for the duration of the cutscene.
extern u8 D_8007216C;

/// 0xFF-terminated area-record list applied the first time the altar fires.
extern GpAreaApplyRec D_neo_ark_altar_801800A0;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_altar_8017EF98[];
/// Spawn table for the follow-up task the altar's message task starts.
extern TaskDesc D_neo_ark_altar_8017F088[];
/// Single-entry spawn table for the altar's cutscene-driver task
/// (`func_neo_ark_altar_8017D668`).
extern TaskDesc D_neo_ark_altar_8017EF8C;

void func_neo_ark_altar_8017D974(Task* task);
void func_neo_ark_altar_8017D9E0(Task* task);

/// State table of the room's message task: set-up
/// (`func_neo_ark_altar_8017D974`), an empty per-frame state and `taskKill`.
/// Its bytes open the room's rodata, ahead of the cutscene driver's jump table.
const TaskFuncTable3 D_neo_ark_altar_8017D5C4 = {
    func_neo_ark_altar_8017D974,
    func_neo_ark_altar_8017D9E0,
    taskKill,
};

/// Altar cutscene driver: silences the player's weapon, runs cap command 2,
/// then branches on the cap event key to record the altar choice in game flag
/// 0xD9 before spawning the follow-up task and restoring control.
void func_neo_ark_altar_8017D668(Task* task)
{
    switch (task->state) {
        case 0:
            D_8007216C               = 5;
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
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
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 4:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    GameFlag_SetNibble(0xD9, 0);
                    task->state++;
                    break;
                case 21:
                    GameFlag_SetNibble(0xD9, 1);
                    task->state++;
                    break;
                case 12:
                    task->state = 0xA;
                    break;
            }
            break;
        case 5:
            if ((GameFlag_GetNibble(0xF9) == 0) && (GameFlag_GetNibble(0xDF) == 0)) {
                GameFlag_SetNibble(0xF9, 1);
                Gp_ApplyAreaRecs(&D_neo_ark_altar_801800A0);
            }
            SndEvt_EnqueueType6(0x5514000D, 0, 0);
            task->killCountdown = 0x1E;
            task->state++;
            break;
        case 6:
            func_neo_ark_altar_8017DC40(GameFlag_GetNibble(0xD9) & 0xFF);
            task->killCountdown--;
            if (task->killCountdown <= 0) {
                task->state++;
            }
            break;
        case 7:
            if (GameFlag_GetNibble(0xD9) != 0) {
                Task_SpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 0, 0);
            } else {
                Task_SpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 1, 0);
            }
            task->state++;
            break;
        case 8:
            task->state = 0xA;
            break;
        case 10:
            SetDispMask(1);
            D_8007216C               = 2;
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            taskKill(task);
            break;
    }
}

s32 func_neo_ark_altar_8017D8BC(void)
{
    return 0;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `GpSaveLoc` onto the outgoing one and passes both on to `func_80179B14`.
/// Always returns 1.
s32 func_neo_ark_altar_8017D8C4(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_altar_8017D908(void)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `Gp_PostMsg13EF` posts. When the record's `field_2` is 1 and
/// agrees with the current view (`GameSession.at4.loc.room`) the altar runs CAP
/// command 3; on any other view the same byte starts the overlay's
/// cutscene-driver task through `D_neo_ark_altar_8017EF8C`. Any other byte is
/// ignored, and the outgoing record is never written - this handler only
/// consumes the message.
s32 func_neo_ark_altar_8017D910(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1) {
        if (gGameSession->at4.loc.room == in->field_2) {
            Gp_RunCapCmd1(3);
        } else {
            Task_SpawnFromTable(&D_neo_ark_altar_8017EF8C, 0, 0, 0);
        }
    }
    return 0;
}

/// State 0 of the altar's message task: park the room's message table in
/// `Task::msgTable`, publish the task in pointer slot 7, bring the altar's
/// switch sprites one step towards the choice recorded in game flag 0xD9
/// (`func_neo_ark_altar_8017DC40`), then start the altar task from
/// `D_neo_ark_altar_8017F088` and advance to state 1.
void func_neo_ark_altar_8017D974(Task* task)
{
    task->msgTable = D_neo_ark_altar_8017EF98;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_altar_8017DC40(GameFlag_GetNibble(0xD9) & 0xFF);
    Task_SpawnFromTable(D_neo_ark_altar_8017F088, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room's message task: nothing to do, the task only
/// holds the message table.
void func_neo_ark_altar_8017D9E0(Task* task)
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
