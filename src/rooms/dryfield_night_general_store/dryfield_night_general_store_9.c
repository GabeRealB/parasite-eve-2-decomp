#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80115598;

extern GpMsgEntry D_dryfield_night_general_store_8017E7BC[];

/// Entry state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, advances to the idle state and raises
/// `D_80115598`.
void func_dryfield_night_general_store_8017DE34(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_general_store_8017E7BC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
