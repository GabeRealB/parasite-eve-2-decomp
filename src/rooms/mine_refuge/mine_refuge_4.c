#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern u8         D_8007216C;
extern u8         D_80062735;
extern u8         D_80115598;
extern s32        D_mine_refuge_80182AD8;
extern u8         D_mine_refuge_80182ADC;
extern GpMsgEntry D_mine_refuge_80181884[];

INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017ea68Title);

void func_mine_refuge_8017FE78(u8 arg0);

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_4", func_mine_refuge_8017FC2C);

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_4", func_mine_refuge_8017FCD0);

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_4", func_mine_refuge_8017FD48);

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            D_8007216C = D_mine_refuge_80182ADC;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 1:
            GameFlag_SetNibble(0x12B, 1);
            func_mine_refuge_8017FE78(D_mine_refuge_80182ADC);
            break;
        default:
            return;
    }
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_4", func_mine_refuge_8017FE78);

void func_mine_refuge_8017FF4C(Task* arg0)
{
    arg0->field_24 = D_mine_refuge_80181884;
    Game_SetPtrSlot(arg0, 7);
    D_mine_refuge_80182AD8 = 0;
    D_80062735             = 1;
    arg0->state            = arg0->state + 1;
    D_80115598             = 1;
}
