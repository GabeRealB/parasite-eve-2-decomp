#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8 D_801153F4;
extern u8 D_80115680;

/// `0x13F2` entry of the room's `D_dryfield_night_motel_loft_8017EB1C`
/// `GpMsgEntry[]`: cues the loft's own sound bank id when the cap script
/// reaches command 5.
s32 func_dryfield_night_motel_loft_8017D6C4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x531F0000 | 5, 0, 0);
    }
    return 0;
}

void func_dryfield_night_motel_loft_8017D6F8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_RunCapCmd(GameFlag_GetNibble(0x170) != 0 ? 0x12 : 3, 0);
            D_80115680  = 5;
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state = arg0->state + 1;
                return;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0x1F) {
                GameFlag_SetNibble(0x170, 1);
            }
            D_801153F4 = 0;
            Gp_MsgPlayerWeapon(1);
            taskKill(arg0);
            break;
    }
}
