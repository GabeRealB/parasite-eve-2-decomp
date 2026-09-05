#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017d730.h"

extern u8 D_80115598;

void RoomsShared8017d730(Task* arg0)
{
    arg0->field_24 = &RoomsShared8017d730Msgs;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}
