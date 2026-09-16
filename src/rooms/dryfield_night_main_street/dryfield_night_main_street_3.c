#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80115598;

/// Message table installed at `Task::field_24` by the room task's state 0
/// (ids `0x13EE`-`0x13F1`).
extern GpMsgEntry D_dryfield_night_main_street_801820B0[];

void func_dryfield_night_main_street_8017E118(void);

s32 func_dryfield_night_main_street_8017E054(void)
{
    return 0;
}

s32 func_dryfield_night_main_street_8017E05C(void)
{
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// pointer slot 7, runs the room's message-table pass, then advances state and
/// raises the `D_80115598` flag.
void func_dryfield_night_main_street_8017E064(Task* arg0)
{
    arg0->field_24 = D_dryfield_night_main_street_801820B0;
    Game_SetPtrSlot(arg0, 7);
    func_dryfield_night_main_street_8017E118();
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_dryfield_night_main_street_8017E0B8(void)
{
}
