#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_power_plant_1_8017EB18[];

/// Power plant task entry: parks the room's message table in `Task::field_24`
/// and registers it as the room's slot-7 pointer. On the first visit - while
/// no save flag for the plant is up yet - it also raises `field_69` (the
/// bank-load skip bits) and, unless nibble 0xFB says the plant has already
/// been powered, latches the "power on" state into `Gp_StateF0` before
/// stepping the task on to its next state.
void func_neo_ark_power_plant_1_8017D928(Task* task)
{
    task->field_24 = D_neo_ark_power_plant_1_8017EB18;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->loc.place == 1) {
        gGameSession->flowFlags = 1;
    }
    if (GameFlag_GetNibble(0xFB) == 0) {
        gGameSession->field_126 = 1;
        Gp_StateF0.field_0      = 2;
    }
    task->state = (s32)(task->state + 1);
}
