#include "common.h"

#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern s16 D_80114D08;
extern s32 D_acropolis_bridge_801917A8;

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F280);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F2D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F358);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F404);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F460);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F4CC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F544);

void func_acropolis_bridge_8017F658(Task* task)
{
    Display_ReleaseRef();
    func_acropolis_bridge_8017E60C(0xFFF, 0);
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, D_acropolis_bridge_801917A8);
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_80114D08             = 0xA;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F6D4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F788);
