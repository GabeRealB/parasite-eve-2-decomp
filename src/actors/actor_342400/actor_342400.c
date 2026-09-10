#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162084);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801621D8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162324);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801624A4);

void func_actor_342400_801626AC(Task* arg0, s32 arg1, Actor342400Msg* arg2)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;
    u16              id   = arg2->field_2;

    if (id == 4) {
        work->field_4 = id;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801626CC);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162748);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801627C0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162824);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162888);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801628F0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_8016299C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162A34);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162AB0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162B60);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162C10);

void func_actor_342400_80162CA8(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162CBC);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162DA0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162E6C);

void func_actor_342400_80162F08(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162F1C);

void func_actor_342400_80162FFC(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80163010);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801630A4);

void func_actor_342400_80163178(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;

    func_actor_342400_801621D8(arg0);
    if (work->field_A == 3) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
    }
}

extern s8 D_80187329;

void func_actor_342400_801631DC(s16 arg0)
{
    if (arg0 == 0) {
        D_80187329 = 2;
        return;
    }
    D_80187329 = 0x11;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80163200);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801632D4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80163354);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801637DC);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801639A8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80163C58);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80163E70);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E68);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E80);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801640B0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_8016454C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801646B8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801648E4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80164CA4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80164DD4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80164F3C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_8016513C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801652A0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161ED4);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161EE0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161EEC);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F00);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F14);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801653DC);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80165CC0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F50);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", jtbl_actor_342400_80161F78);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F8C);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", jtbl_actor_342400_80161FA0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161FB4);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161FC0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161FCC);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161FD8);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161FE8);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80162010);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80162028);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_8016203C);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", jtbl_actor_342400_80162058);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", jtbl_actor_342400_80162070);
