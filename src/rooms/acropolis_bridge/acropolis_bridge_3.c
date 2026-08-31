#include "common.h"
#include "main/task.h"
extern s32 D_acropolis_bridge_801917A0;

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DA0C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DA64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DB08);

void func_acropolis_bridge_8017DB60(Task* arg0)
{
    Gp_StartCapSlot(7, 1, 1);
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DBA0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DC1C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_3", func_acropolis_bridge_8017DC68);

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
