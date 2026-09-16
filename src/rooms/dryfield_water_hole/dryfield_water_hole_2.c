#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_water_hole_8017FC5C[];
extern TaskDesc   D_dryfield_water_hole_8017FC8C[];

s32 func_dryfield_water_hole_8017D78C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 4:
            SndEvt_EnqueueType6(0x52200004, 0, 0);
            break;
        case 5:
            SndEvt_EnqueueType6(0x52200005, 0, 0);
            break;
    }
    return 0;
}

void func_dryfield_water_hole_8017D7DC(Task* arg0)
{
    arg0->field_24 = D_dryfield_water_hole_8017FC5C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_water_hole_8017FC8C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_water_hole_8017D838(void)
{
}
