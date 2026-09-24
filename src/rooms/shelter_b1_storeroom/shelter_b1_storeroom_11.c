#include "common.h"

#include "rooms/shelter_b1_storeroom.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80115598;

void func_shelter_b1_storeroom_8017D740(Task* arg0)
{
    arg0->msgTable = &D_shelter_b1_storeroom_80184968;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
