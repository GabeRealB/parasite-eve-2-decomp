#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8  D_80115598;
extern s32 D_dryfield_cellar_8017DB8C;

s32 func_dryfield_cellar_8017D6F4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_cellar/dryfield_cellar_2", func_dryfield_cellar_8017D6FC);

void func_dryfield_cellar_8017D730(Task* arg0)
{
    arg0->field_24 = &D_dryfield_cellar_8017DB8C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_dryfield_cellar_8017D77C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_cellar/dryfield_cellar_2", func_dryfield_cellar_8017D784);
