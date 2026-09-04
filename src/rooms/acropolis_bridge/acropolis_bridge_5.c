#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_acropolis_bridge_80189234;
extern SVECTOR  D_acropolis_bridge_80189240[];
extern Task*    D_acropolis_bridge_8019179C;
extern u16      D_acropolis_bridge_801917A4;

void func_acropolis_bridge_8017F2D0(s32 arg0);

void func_acropolis_bridge_8017DD9C(Task* arg0)
{
    Task* task = Task_SpawnFromTable(&D_acropolis_bridge_80189234, 0, 0, 0);
    s32   next = arg0->state + 1;

    D_acropolis_bridge_8019179C = task;
    arg0->state                 = next;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017DDEC);

void func_acropolis_bridge_8017DE94(Task* arg0)
{
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
    Game_Session->field_68 = 0;
    arg0->state            = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017DEE4);

s16 func_acropolis_bridge_8017E024(void)
{
    return D_acropolis_bridge_80189240[D_acropolis_bridge_801917A4 + 1].vy;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E04C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E1D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E3A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E4FC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E60C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E81C);
