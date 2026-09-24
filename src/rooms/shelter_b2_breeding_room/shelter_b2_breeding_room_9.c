#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80115598;

/// Message table `func_shelter_b2_breeding_room_8017D7EC` installs on its task.
extern GpMsgEntry D_shelter_b2_breeding_room_80180414[];

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7, sets `D_80115598` and advances to the next state.
void func_shelter_b2_breeding_room_8017D7EC(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_breeding_room_80180414;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
