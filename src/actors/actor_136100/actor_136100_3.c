#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern s16      D_actor_136100_8013F17C;
extern TaskDesc ActorsShared80134898Desc;
extern Task*    D_actor_136100_8014078C;

void func_actor_136100_801348C8(void)
{
    Task_SpawnFromTable(&ActorsShared80134898Desc, 5, 9, 0);
}

void func_actor_136100_801348F8(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

void func_actor_136100_80134924(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    if (work->field_4EC == 0) {
        work->field_4EC = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_136100_80134964(void)
{
    Actor136100Work* work = (Actor136100Work*)D_actor_136100_8014078C->idMap;

    if (work->field_4EC != 0) {
        Gp_SpawnWeaponEff();
        work->field_4EC = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_3", func_actor_136100_801349B4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_3", func_actor_136100_80134A18);
