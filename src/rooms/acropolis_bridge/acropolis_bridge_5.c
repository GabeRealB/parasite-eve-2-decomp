#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

void func_acropolis_bridge_8017F2D0(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017DD9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017DDEC);

void func_acropolis_bridge_8017DE94(Task* arg0)
{
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
    Game_Session->field_68 = 0;
    arg0->state            = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017DEE4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E024);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E04C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E1D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E3A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E4FC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E60C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E81C);
