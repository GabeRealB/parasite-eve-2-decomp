#include "common.h"

#include "gameplay/1BC.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void  func_800B1EFC(Task* arg0);
void  func_800B3448(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800B3910(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800B3F60(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
Task* func_8002CFA0(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);

extern TaskDesc D_80119218[];

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF590);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF89C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFA44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFF90);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0034);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B00C4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0118);

void func_800B015C(void* arg0)
{
    CdCmd_Queue.field_198 = arg0;
}

GpEnemy* func_800B0168(s32 bank, s32 type, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_Spawn(bank, type, arg2, 0);
    if (task != NULL) {
        ret = func_800B0494(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

GpEnemy* func_800B01AC(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = func_8002CFA0(table, idx, arg2, 0);
    if (task != NULL) {
        ret = func_800B0494(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

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

void func_800B28E0(Task* task)
{
    task->killCountdown = 0x20;
    task->state++;
    func_800B1EFC(task);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2910);

Task* func_800B2968(void)
{
    return func_8002CFA0(D_80119218, 0, 0, 0);
}

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

void func_800B3F84(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    func_800B3F60(arg0, arg1, arg2, arg3, arg4);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3FA8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B404C);

void func_800B40F4(s32 arg0, s32 arg1, s32 arg2)
{
    func_800B3910(arg0, arg1, arg2, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4114);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4248);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B43E0);

void func_800B4514(s32 arg0, s32 arg1)
{
    func_800B3448(arg0, arg1, 0, 0);
}

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

void func_800B58D4(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->field_24 = arg1;
    arg0->field_25 = arg2;
    if (arg0->field_18 != NULL) {
        Tmd_ProcessStream(arg0);
        Tmd_ProcessStream(arg0);
    }
}

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

void func_800B6398(void)
{
    func_800D9DFC();
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B63B8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B65B0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6950);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6B44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6CF0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6DA4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6EE0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B715C);
