#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8 D_801153F4;
extern u8 D_80115680;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_2", func_dryfield_night_motel_loft_8017D6C4);

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
            Task_Kill(arg0);
            break;
    }
}
