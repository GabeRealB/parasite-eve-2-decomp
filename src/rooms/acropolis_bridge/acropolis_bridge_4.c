#include "common.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
extern s8 D_8007216C;

extern s32 D_acropolis_bridge_801917A0;

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_4", func_acropolis_bridge_8017DB08);

void func_acropolis_bridge_8017DB60(Task* arg0)
{
    Gp_StartCapSlot(7, 1, 1);
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_bridge_8017DBA0(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA3, 0, 0) == 0) {
        D_8007216C             = 8;
        Game_Session->field_68 = 1;
        Gp_MsgPlayer3F3(0);
        Gp_MsgPlayerWeapon(0);
        arg0->state = (s32)(arg0->state + 1);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_4", func_acropolis_bridge_8017DC1C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_4", func_acropolis_bridge_8017DC68);

void func_acropolis_bridge_8017DD24(Task* arg0)
{
    if (D_acropolis_bridge_801917A0 == 0) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Task_Kill(arg0);
        return;
    }
    Gp_MsgPlayer3F3(1);
    arg0->state += 1;
}
