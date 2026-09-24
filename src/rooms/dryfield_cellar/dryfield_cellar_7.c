#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_cellar_8017DB8C[];

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7, moves on to the next state and sets the
/// gameplay byte `D_80115598`.
void func_dryfield_cellar_8017D730(Task* arg0)
{
    arg0->msgTable = D_dryfield_cellar_8017DB8C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
