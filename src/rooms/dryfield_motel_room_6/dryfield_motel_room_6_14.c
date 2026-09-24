#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8 D_80115598;

/// The room's message table, installed on the room entry task.
extern s32 D_dryfield_motel_room_6_80182D48;

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7, advances the state and sets the
/// gameplay byte `D_80115598`.
void func_dryfield_motel_room_6_80181AC4(Task* arg0)
{
    arg0->msgTable = &D_dryfield_motel_room_6_80182D48;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
