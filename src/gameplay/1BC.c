#include "common.h"

#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

void func_800B3910(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF590);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF89C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFA44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFF90);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0034);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B00C4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0118);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B015C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0168);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B01AC);

void func_800B01F0(GpEnemy* enemy, Task* task)
{
    func_800DAB38(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

void func_800B0234(Task* task)
{
    GpEnemy* enemy;

    enemy = task->spawnArg2;
    func_800DAB38(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0278);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0494);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0544);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0560);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B058C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B05E8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B065C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B06F0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0748);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B082C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B08D8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0928);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0CF4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0FDC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B114C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B1460);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B17D4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B1D00);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B1EFC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2088);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2200);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B25B0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B27C4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2840);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B28E0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2910);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2968);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2998);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2E90);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3108);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B32E8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3448);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3910);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3AA4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3CCC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3CE8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3DB4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3DF4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3E34);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3E74);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3EE8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3F60);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3F84);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3FA8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B404C);

void func_800B40F4(s32 arg0, s32 arg1, s32 arg2)
{
    func_800B3910(arg0, arg1, arg2, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4114);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4248);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B43E0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4514);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4538);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4668);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B46A4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4754);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B47A8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B48FC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4AF8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4E54);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B51F4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B56AC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B57EC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B584C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B58D4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5914);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B59A8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5A08);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5A48);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5B30);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5BFC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5C88);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5CE8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5D44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5DB8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5E08);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5E78);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5EE8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5F5C);

void func_800B5FEC(void)
{
    Task_KillChildren(Game_GetPtrSlot(4));
}

void func_800B6014(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B601C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6094);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B60C0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6118);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B62D4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6358);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6398);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B63B8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B65B0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6950);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6B44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6CF0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6DA4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6EE0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B715C);
