#include "common.h"

#include "actors/actor_113100.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/1BC.h"

extern void func_actor_113100_80132790(Task*, s32, s32, s32);
extern void func_80183BAC(s32);

void func_actor_113100_80132F40(Task* arg0)
{
    Actor113100Work* work;
    s32              flag;

    work = (Actor113100Work*)arg0->idMap;
    flag = GameFlag_GetNibble(0xED);
    if (flag > 0 && work->field_53E == 0) {
        func_actor_113100_80132790(arg0, 0, 1, 0);
        func_80183BAC(0);
    }
    work->field_53E = flag;
}

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100_2", func_actor_113100_80132FB4);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100_2", func_actor_113100_8013301C);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100_2", func_actor_113100_801330E8);

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100_2", func_actor_113100_801331E8);
