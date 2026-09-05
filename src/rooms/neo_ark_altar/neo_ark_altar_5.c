#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc RoomsShared8018397cDesc;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DC40);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DF0C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E148);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E260);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E658);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E92C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EC34);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017ECE0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017ED60);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EDBC);

void func_neo_ark_altar_8017EDF8(Task* arg0)
{
    Gp_MsgPlayerWeapon(0);
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EE30);

void func_neo_ark_altar_8017EE90(Task* arg0)
{
    Task** temp_s1;

    temp_s1 = arg0->idMap;
    Gp_MsgPlayer3F3(0);
    Game_Session->field_68 = 1;
    *temp_s1               = Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, 2, 0);
    arg0->state            = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EF00);

void func_neo_ark_altar_8017EF34(Task* arg0)
{
    SetDispMask(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayerWeapon(1);
    Game_Session->field_68 = 0;
    arg0->state            = 2;
}

void func_neo_ark_altar_8017EF84(void)
{
}
