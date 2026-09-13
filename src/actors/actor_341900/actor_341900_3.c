#include "common.h"

#include "main/task.h"

#include "actors/actor_341900.h"

extern TaskDesc D_actor_341900_80164190;

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_80163388);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_801633C0);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_801633F8);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_80163438);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_80163488);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_801634D0);

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(&D_actor_341900_80164190, 1, 9, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_80163564);

void func_actor_341900_80163584(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->idMap;

    work->field_64 = arg0;
    work->field_66 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900_3", func_actor_341900_801635A4);
