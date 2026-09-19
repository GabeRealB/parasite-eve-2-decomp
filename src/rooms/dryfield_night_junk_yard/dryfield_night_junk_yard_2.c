#include "common.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// The room task's message table, published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk (`func_dryfield_night_junk_yard_8017D82C` is its
/// 0x13EF entry) and the four-byte payload the announcement below carries.
extern GpMsgEntry D_dryfield_night_junk_yard_8018055C[];
extern s32        D_dryfield_night_junk_yard_801805A0;

void func_dryfield_night_junk_yard_8017D9B8(u8 arg0);

/// Room entry task tick: publish the message table, claim game pointer slot 7,
/// and, on the visit whose sub-id (`gGameSession::at4.loc.place`) is 1 and that has
/// already latched nibble 0x9F, announce the room to the slot-4 task with
/// message 0x7DA. The nibble is then applied to the current sprite-table entry
/// either way, and the state advances.
void func_dryfield_night_junk_yard_8017D8B0(Task* task)
{
    u8 subId;

    task->msgTable = &D_dryfield_night_junk_yard_8018055C;
    Game_SetPtrSlot(task, 7);
    subId = gGameSession->at4.loc.place;
    if (subId == 1 && GameFlag_GetNibble(0x9F) == subId) {
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_night_junk_yard_801805A0, 0x7DB);
    }
    func_dryfield_night_junk_yard_8017D9B8(GameFlag_GetNibble(0x9F));
    task->state = task->state + 1;
}

void func_dryfield_night_junk_yard_8017D958(void)
{
}
