#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8  D_80115598;
extern s32 D_dryfield_night_general_store_8017E7BC;

s32 func_dryfield_night_general_store_8017DE24(void)
{
    return 0;
}

s32 func_dryfield_night_general_store_8017DE2C(void)
{
    return 0;
}

void func_dryfield_night_general_store_8017DE34(Task* arg0)
{
    arg0->field_24 = &D_dryfield_night_general_store_8017E7BC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_dryfield_night_general_store_8017DE80(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_general_store/dryfield_night_general_store_3", func_dryfield_night_general_store_8017DE88);
