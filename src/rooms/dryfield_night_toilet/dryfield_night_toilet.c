#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_dryfield_night_toilet_8017DA70[];
extern TaskDesc   D_8013E51C;

s32 func_dryfield_night_toilet_8017D678(void)
{
    return 0;
}

s32 func_dryfield_night_toilet_8017D680(void)
{
    return 0;
}

s32 func_dryfield_night_toilet_8017D688(void)
{
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::field_24`,
/// register the task in pointer slot 7, then - on the visit whose session
/// sub-id (`Game_Session::field_9`) is 1 and that has not yet latched nibble
/// 0xAF - latch the nibble and start the follow-up task. Advances to state 1
/// either way.
void func_dryfield_night_toilet_8017D690(Task* task)
{
    task->field_24 = D_dryfield_night_toilet_8017DA70;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xAF) == 0 && Game_Session->field_9 == 1) {
        GameFlag_SetNibble(0xAF, 1);
        Task_SpawnFromTable(&D_8013E51C, 0, 0, 0);
    }
    task->state = task->state + 1;
}

void func_dryfield_night_toilet_8017D71C(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_toilet/dryfield_night_toilet", D_dryfield_night_toilet_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_toilet/dryfield_night_toilet", RoomsShared8017d878Table);
