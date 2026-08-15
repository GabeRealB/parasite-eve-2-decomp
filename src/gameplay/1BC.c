#include "common.h"

#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_800B1EFC(Task* arg0);
void func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800B3910(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800B6358(Task* task);
void func_800B6398(void);
void func_800A8864(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void func_8017FBD8(void);

extern TaskDesc D_80115D9C[];
extern TaskDesc D_80119218[];
extern TaskDesc D_8011922C[];
extern TaskDesc D_801637C8[];
extern TaskDesc D_8017D9E8[];
extern TaskDesc D_80180DBC[];
extern TaskDesc D_801810E4[];
extern TaskDesc D_80181398[];
extern TaskDesc D_80181638[];
extern TaskDesc D_8018186C[];
extern TaskDesc D_80181B30[];
extern TaskDesc D_80181B88[];
extern TaskDesc D_80181F18[];
extern TaskDesc D_80182D0C[];
extern TaskDesc D_80182E74[];
extern TaskDesc D_80182FAC[];
extern TaskDesc D_8018384C[];
extern s32      D_8010D208[];
extern char     D_800939F8[];
extern s32      D_80070F10;
extern s32      D_80070F60;
extern u8       D_800626E8;
extern s32      D_80114D20;

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF590);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF89C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFA44);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFF90);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0034);

void func_800B00C4(void)
{
    CdCmdQueue* p;

    p            = &CdCmd_Queue;
    p->field_1FE = 0xFF;
    p->field_23A = 1;
    p->field_214 = 0;
    p->field_212 = 0;
    p->field_216 = 0;
    p->field_240 = 0;
    D_80070F60   = p->field_1A8;
    srand(p->field_1AC);
}

s32 func_800B0118(s32 arg0, s32 arg1)
{
    s16 temp;

    temp = arg0;
    if (temp != 0) {
        D_80114D20  = temp;
        D_8005EC80 |= 8;
    } else {
        D_8005EC80 &= ~8;
    }
    return 0;
}

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

    task = Task_SpawnFromTable(table, idx, arg2, 0);
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

GpEnemy* func_800B0494(Task* task, GpEnemy* parent)
{
    GpEnemy* enemy;

    enemy = Mem_Calloc(0x60, 0);
    if (enemy == NULL) {
        printf(D_800939F8);
        Task_Kill(task);
        return NULL;
    }

    task->exitCallback = func_800B0234;
    task->spawnArg2    = enemy;
    enemy->task        = task;
    enemy->field_18    = &D_80070F10;
    if (parent != NULL) {
        Task_Reparent(parent->task, task);
    } else {
        Task_Reparent(Game_GetPtrSlot(4), enemy->task);
    }
    return enemy;
}

void func_800B0544(GpEnemy* enemy, Task* task)
{
    enemy->field_C = 0x78;
    task->state++;
}

void func_800B0560(GpEnemy* enemy, Task* task)
{
    enemy->field_C--;
    if (enemy->field_C == 0) {
        task->state++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B058C);

s32 func_800B05E8(s32 arg0)
{
    u8 param1[8];
    u8 param2[8];

    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[0] = arg0;
        param1[3] = 0;
        param1[2] = 5;
        param2[0] = 1;
        param2[1] = 1;
        param2[3] = 0;
        param2[2] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        D_800626E8 = 1;
        return 0;
    }
    return 0xFF;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B065C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B06F0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0748);

void func_800B082C(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Display_State.field_100 = 1;
        if (Game_Session->field_128 == 0xFF) {
            Task_SpawnFromTable(D_8011922C, 0, 0, 0);
            Task_Kill(task);
        } else {
            task->spawnArg2 = Task_SpawnFromTable(D_80115D9C, 0, 0, 0);
            SndEvt_EnqueueType1(0x62, 0);
        }
        task->state++;
    }
}

void func_800B08D8(Task* task)
{
    s32           out;
    DisplayState* ds;

    if (Task_PollKill(task->spawnArg2, &out) != 0) {
        ds                  = &Display_State;
        task->killCountdown = 0;
        ds->field_11e       = 1;
        ds->field_12f       = 0;
        Task_Kill(task);
    }
}

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

void func_800B25B0(void)
{
    switch (*(u32*)&Mc_SaveData.field_4 & 0xFFFF0000) {
        case 0x51B0000:
            Task_SpawnFromTable(D_80181F18, 0, 0, 0);
            break;
        case 0x50F0000:
            Task_SpawnFromTable(D_80181398, 0, 0, 0);
            break;
        case 0x50E0000:
            Task_SpawnFromTable(D_80181B30, 0, 0, 0);
            break;
        case 0x50D0000:
            Task_SpawnFromTable(D_8018384C, 0, 0, 0);
            break;
        case 0x50C0000:
            Task_SpawnFromTable(D_801810E4, 1, 0, 0);
            break;
        case 0x5070000:
            Task_SpawnFromTable(D_80180DBC, 0, 0, 0);
            break;
        case 0x21E0000:
            Task_SpawnFromTable(D_80182D0C, 0, 1, 0);
            break;
        case 0x31E0000:
            Task_SpawnFromTable(D_80182E74, 0, 1, 0);
            break;
        case 0x4120000:
            Task_SpawnFromTable(D_80181B88, 0, 0, 0);
            break;
        case 0x51F0000:
            Task_SpawnFromTable(D_8017D9E8, 0, 0, 0);
            break;
        case 0x51E0000:
            Task_SpawnFromTable(D_8018186C, 0, 0, 0);
            Task_SpawnFromTable(D_8018186C, 1, 0, 0);
            break;
        case 0x51D0000:
            Task_SpawnFromTable(D_80181638, 0, 0, 0);
            break;
        case 0x4160000:
            Task_SpawnFromTable(D_801637C8, 0, 0, 0);
            break;
        case 0x4300000:
            Task_SpawnFromTable(D_80182FAC, 0, 0, 0);
            break;
        case 0x1140000:
            func_8017FBD8();
            break;
    }
}

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
    return Task_SpawnFromTable(D_80119218, 0, 0, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2998);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2E90);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3108);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B32E8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3448);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3910);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3AA4);

void func_800B3CCC(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3CE8);

void func_800B3DB4(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void func_800B3DF4(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void func_800B3E34(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3E74);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3EE8);

void func_800B3F60(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
    arg0->field_C  = arg4;
}

void func_800B3F84(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
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

void func_800B4514(GpAnimCtx* arg0, s32 arg1)
{
    func_800B3448(arg0, arg1, 0, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4538);

s32* func_800B4668(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u16  idx;
    s32* ret;

    idx = arg1->field_0;
    switch (idx) {
        case 0x7FFF:
            return NULL;
        default:
            ret  = arg1->field_20[idx]->field_0;
            ret += arg1->field_2;
            return ret;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B46A4);

void func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3)
{
    u16 limit;

    limit = arg1->field_20[arg2]->field_4[arg1->field_15];
    if (arg3 < limit) {
        arg3            = limit;
        arg1->field_10 |= 1;
    }
    arg1->field_2 = arg3;
    arg1->field_0 = arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B47A8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B48FC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4AF8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4E54);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B51F4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B56AC);

void func_800B57EC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1)
{
    GsCOORDINATE2* dest;

    dest = arg1;
    if (dest->sub != arg0) {
        func_80098F58(arg0);
        func_80098F58(dest);
        dest->sub = arg0;
        func_800A8864(&arg0->workm, &dest->workm, &dest->coord);
        dest->flg = 0;
    }
}

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

GpAreaObj* func_800B5A08(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = D_8010CBCC[arg0->field_3];
    if (rec == NULL) {
        ret = NULL;
    } else {
        ret = rec[arg0->field_2].field_4;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5A48);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5B30);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5BFC);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5C88);

GpAreaRec* func_800B5CE8(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaRec* ret;

    rec = D_8010CBCC[arg0->field_3];
    ret = NULL;
    if (rec != NULL) {
        ret = rec[arg0->field_2].field_0;
        if (ret != NULL) {
            ret = &ret[arg0->field_5];
        }
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5D44);

void func_800B5DB8(Task* arg0)
{
    TaskFunc funcs[2] = { func_800B6358, func_800B6398 };

    funcs[arg0->state](arg0);
}

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

void func_800B6094(Task* task)
{
    if (task->spawnArg1 & 1) {
        task->killCountdown = 0;
    }
    task->state++;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B60C0);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B6118);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B62D4);

void func_800B6358(Task* task)
{
    Game_SetPtrSlot(task, 4);
    task->field_24 = D_8010D208;
    task->state++;
}

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
