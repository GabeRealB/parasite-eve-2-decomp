#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_altar_8017EF98[];
/// Spawn table for the follow-up task the altar's message task starts.
extern TaskDesc D_neo_ark_altar_8017F088[];
/// Single-entry spawn table for the altar's cutscene-driver task
/// (`func_neo_ark_altar_8017D668`).
extern TaskDesc D_neo_ark_altar_8017EF8C;

void func_neo_ark_altar_8017DC40(s32 arg0);

s32 func_neo_ark_altar_8017D908(void)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `Gp_PostMsg13EF` posts. When the record's `field_2` is 1 and
/// agrees with the current view (`GameSession.field_5`) the altar runs CAP
/// command 3; on any other view the same byte starts the overlay's
/// cutscene-driver task through `D_neo_ark_altar_8017EF8C`. Any other byte is
/// ignored, and the outgoing record is never written - this handler only
/// consumes the message.
s32 func_neo_ark_altar_8017D910(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1) {
        if (Game_Session->field_5 == in->field_2) {
            Gp_RunCapCmd1(3);
        } else {
            Task_SpawnFromTable(&D_neo_ark_altar_8017EF8C, 0, 0, 0);
        }
    }
    return 0;
}

/// State 0 of the altar's message task: park the room's message table in
/// `Task::field_24`, publish the task in pointer slot 7, record the choice made
/// at the altar (game flag 0xD9) through the cutscene driver, then start the
/// follow-up task and advance to state 1.
void func_neo_ark_altar_8017D974(Task* task)
{
    task->field_24 = D_neo_ark_altar_8017EF98;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_altar_8017DC40(GameFlag_GetNibble(0xD9) & 0xFF);
    Task_SpawnFromTable(D_neo_ark_altar_8017F088, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

void func_neo_ark_altar_8017D9E0(void)
{
}
