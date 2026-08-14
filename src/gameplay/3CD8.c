#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

extern WipSysConfig D_80073B88;
extern TaskDesc     D_8010FAEC[];
extern s32          D_801155A8;
extern Task*        D_80115674;
extern s16          D_80115654;
extern s16          D_80115656;
extern u8           D_8011565A;
extern s32          D_80115660;
extern s16          D_80115668;
extern s32          D_8011568C;
extern s16          D_80115698;
extern s16          D_8011569A;
extern u8           D_8011569C;
extern s32          D_801156A8;
extern s8           D_801156B0;
extern u8           D_80115700;
extern u8           D_80115701;
extern u8           D_80115702;

s32   func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
Task* func_8002CFA0(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
void  func_800E34D8(s32 arg0, s16 arg1);
void  func_800E646C(Task* arg0);
s32   func_800E6C70(s16 arg0, s16 arg1, s16 arg2);
s32   func_800E6CE0(void);
s32   func_800E6CF0(void);
void  func_800E6D60(s32 arg0);
void  func_8003F6F8(void);
void  func_800E8634(s32 arg0, s32 arg1, s32 arg2);
void  func_800E8A90(Task* task);
void  func_800E8BB0(Task* task);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E34D8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E375C);

void func_800E3B3C(s32 arg0, s32 arg1)
{
    if (arg0 != 0) {
        GameFlag_SetNibble(arg0, arg1);
    }
}

void func_800E3B60(s32 arg0)
{
    func_800E34D8(arg0, 1);
}

void func_800E3B80(s32 arg0)
{
    func_800AC464(Game_GetPtrSlot(3), 0x3F3, arg0, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3BBC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3C6C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3CEC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3D24);

void func_800E3D5C(void)
{
    WipSysConfig* p;

    p           = &D_80073B88;
    p->field_18 = p->field_1a;
    p->field_1c = p->field_1e;
}

void func_800E3D78(void)
{
    Mc_SaveData.field_6C8 = Mc_SaveData.field_6CA;
}

void func_800E3D8C(s32 arg0, s32 arg1)
{
    if (func_800E6CE0() == 0) {
        func_8002CFA0(D_8010FAEC, 0, arg1, arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3DD8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3E30);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3E64);

void func_800E3EB0(s32 arg0)
{
    Task* slot;

    slot = Game_GetPtrSlot(0xA);
    if (slot != NULL) {
        func_800AC464(slot, 0x3F3, arg0, 0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E3EF0);

void func_800E3FAC(s32 arg0, s32 arg1)
{
    D_80073980[arg0 / 2 + 4] = arg1;
}

s32 func_800E3FCC(s32 arg0)
{
    return D_80073980[arg0 / 2 + 4];
}

s32 func_800E3FF0(s32 arg0, s32 arg1, GBytes8* arg2, GBytes8* arg3)
{
    *arg3 = *arg2;
    return 1;
}

s32 func_800E4018(void)
{
    return 0;
}

void func_800E4020(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E4028);

void func_800E4080(void)
{
    s32 i;

    for (i = 0; i < 0x1F8; i++) {
        GameFlag_SetNibble(i, 0);
    }
}

void func_800E40BC(s32 arg0, s32 arg1)
{
    func_8002CFA0(D_8010FAEC, 1, arg0, arg1);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E40EC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E41F4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E44A0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E5578);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E62C0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E646C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6608);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E67C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E68D8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E69F4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6AD4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6BB8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6C70);

s32 func_800E6CE0(void)
{
    return D_801155A8 != 0;
}

s32 func_800E6CF0(void)
{
    if (D_801155A8 != 0) {
        if (D_80115674 != NULL) {
            func_800E646C(D_80115674);
            return 0;
        }
        return -1;
    }
    return -1;
}

s32 func_800E6D3C(void)
{
    return D_80115668;
}

void func_800E6D4C(s16 arg0, s16 arg1)
{
    D_80115654 = arg0;
    D_80115656 = arg1;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6D60);

void func_800E6DF4(void)
{
    D_801155A8 = 0;
    D_801156A8 = 0;
    D_8011565A = 0;
    func_800E6D4C(0x180, 0);
    D_8011568C = 0;
    func_800E6D60(0);
    D_8011569C = 0;
}

void func_800E6E44(s32 arg0)
{
    D_80115660 = arg0;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6E50);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6EA0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6EF4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6F60);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E704C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E70AC);

void func_800E712C(Task* task)
{
    GpEndWait* flag;

    flag = task->spawnArg2;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x2C, 0, (s32)flag);
            task->state++;
            break;
        case 1:
            if (flag->field_2 != 0) {
                func_8003F6F8();
                Task_Kill(task);
            }
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E71B0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E7240);

s32 func_800E72E8(s32 arg0, s32 arg1, s16 arg2)
{
    func_800E6C70(arg2, 0, 0);
    D_801156B0 = 0;
    return 0;
}

s32 func_800E731C(void)
{
    D_8011569A = 0;
    D_80115698 = 0;
    return 0;
}

s32 func_800E7334(void)
{
    D_801156B0 = 0;
    return func_800E6CF0();
}

s32 func_800E7358(void)
{
    return func_800E6CE0();
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E7378);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E73E8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E7434);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E7498);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E74EC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E7570);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E75C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8378);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E84B8);

void func_800E8614(s32 arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8634);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E86FC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8758);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8830);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8888);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8938);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8A90);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8BB0);

void func_800E8CE8(s16 arg0)
{
    if (arg0 != 0) {
        Task_Spawn(2, 0xB, arg0, 0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8D1C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8E00);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8F68);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8FB0);

void func_800E9034(Task* task)
{
    func_800E8A90(task);
    func_800E8BB0(task);
    task->state++;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9070);

void func_800E916C(void)
{
    D_80115700 = 0;
    D_80115701 = 0;
    D_80115702 = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9188);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9218);

void func_800E92BC(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E92C4);

void func_800E9308(Task* task)
{
    func_800E8A90(task);
}

void func_800E9328(Task* task)
{
    func_800E8A90(task);
}

void func_800E9348(void)
{
}

void func_800E9350(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_11 == 0) {
        func_800E8BB0(task);
    }
}

void func_800E9394(Task* task)
{
    func_800E8BB0(task);
}

void func_800E93B4(Task* task)
{
    func_800E8BB0(task);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E93D4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9498);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E956C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9A50);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9BDC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9C6C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9CC8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9EFC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA02C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA1A8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA318);

void func_800EA3A0(s32 arg0)
{
    D_80115740->field_C = arg0 + 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA3B4);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA3EC);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA420);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA478);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EA858);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EAA0C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EAEB8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EB2C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EB6E8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EB9B0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EBF18);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EC47C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EC674);

void func_800EC7E4(void* arg0, Task* arg1)
{
    D_80115740->field_0--;
    Mem_Free(arg0);
    Task_Kill(arg1);
}

void func_800EC824(Task* arg0)
{
    void* mem;

    mem = arg0->spawnArg2;
    D_80115740->field_0--;
    Mem_Free(mem);
    Task_Kill(arg0);
}

void func_800EC868(void)
{
    D_80115740->field_1A |= 0x100;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EC888);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EC914);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800EC9C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800ECA10);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800ECA54);
