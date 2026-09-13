#include "common.h"

#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_341900.h"

extern TaskDesc D_actor_341900_80164190;

void func_actor_341900_80163388(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    Gp_DispatchMsg(work->field_8, 0x7D5, arg0, 0);
}

void func_actor_341900_801633C0(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    Gp_DispatchMsg(work->field_0, 0x3F3, arg0, 0);
}

void func_actor_341900_801633F8(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    if (work->field_6C == 0) {
        work->field_6C = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_341900_80163438(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    if (work->field_6C != 0) {
        Gp_SpawnWeaponEff();
        work->field_6C = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_341900_80163488(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    if (work->field_8 != NULL) {
        Task_Kill(work->field_8);
        work->field_8 = NULL;
    }
}

void func_actor_341900_801634D0(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    if (work->field_C != NULL) {
        Task_Kill(work->field_C);
        work->field_C = NULL;
    }
    if (work->field_10 != NULL) {
        Task_Kill(work->field_10);
        work->field_10 = NULL;
    }
}

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(&D_actor_341900_80164190, 1, 9, 0);
}

void func_actor_341900_80163564(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    work->field_5C = arg0;
    work->field_5E = 0;
}

void func_actor_341900_80163584(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    work->field_64 = arg0;
    work->field_66 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_801635A4);
