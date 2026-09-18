#include "common.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// One byte of gameplay state, written by room script tables and read back with
/// `lb` by the field text actor (`actor_101600_text`), so it is signed.
extern s8 D_8011540C;

/// The parking lot's own `GpMsgEntry[]` - the message table its cap scripts
/// index. Its first handler is `func_dryfield_night_parking_lot_8017D8D0`.
extern GpMsgEntry D_dryfield_night_parking_lot_8017EC60[];

/// Parking-lot task entry: parks the room's message table in `Task::msgTable`
/// and publishes the task in pointer slot 7, as the shared `RoomsShared8017db84`
/// does for most rooms. On the phase-3 visit where nibble 0x79 is set - the one
/// `func_dryfield_night_parking_lot_8017DB34` arms when the player first takes a
/// weapon - it also latches `D_8011540C` to 2, then steps the task on.
void func_dryfield_night_parking_lot_8017DBB0(Task* task)
{
    task->msgTable = D_dryfield_night_parking_lot_8017EC60;
    Game_SetPtrSlot(task, 7);
    if ((gGameSession->at4.loc.place == 3) && (GameFlag_GetNibble(0x79) != 0)) {
        D_8011540C = 2;
    }
    task->state = (s32)(task->state + 1);
}

void func_dryfield_night_parking_lot_8017DC28(void)
{
}
