#include "common.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"

extern u8 D_80071068; // Display_State.field_100
extern u8 D_8007106B; // Display_State.field_103
extern u8 D_80073BA9;

void func_800A9310(void);
void func_800A9730(Task* task);
void func_800AA548(s32 arg0);
s32  func_800ACF8C(void);

extern TaskDesc D_80183824[];

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9310);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A954C);

void func_800A95E0(Task* task)
{
    if (CdCmd_Queue.field_1FA != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        func_800A9730(task);
    }
}

void func_800A9630(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = -2;
        func_800A9730(task);
    }
}

void func_800A966C(Task* task)
{
    if (CdCmd_Queue.field_1FE == 0xFF) {
        task->state = -1;
        func_800A9730(task);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A96A0);

void func_800A9730(Task* task)
{
    Pad_ClearCooldown(0);
    if (task->spawnArg1 == 0) {
        Stage_RequestSpecialFlag(1);
        Game_Session->field_52 = 0;
        Task_Kill(task);
        Display_ResetHeapWrapper();
    } else {
        if (task->spawnArg1 == 1) {
            D_8007106B = 1;
        }
        D_80071068 = 2;
        Task_Spawn(0, 0x17, 0, 0);
        Game_Session->field_4D = 1;
        Task_Kill(task);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A97DC);

void func_800A987C(void)
{
    Task*       slot;
    McSaveData* save;

    slot            = Game_GetPtrSlot(1);
    save            = &Mc_SaveData;
    slot->spawnArg1 = save->field_4;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Game_Session->field_4 = save->field_4;
    Pad_SetCooldown(0);
    func_800A8DC0(2);
    Game_Session->field_4D = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

void func_800A990C(s32 arg0)
{
    Task* slot;

    slot                  = Game_GetPtrSlot(1);
    D_8007216C            = arg0;
    Game_Session->field_4 = arg0;
    slot->spawnArg1       = (u8)arg0;
    Pad_SetCooldown(0);
    func_800A8DC0(1);
    D_80071068 = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void func_800A9980(Task* task)
{
    u8 val;

    val                   = *(u8*)&task->spawnArg1;
    D_8007216C            = val;
    Game_Session->field_4 = val;
    Task_Kill(task);
}

void func_800A99B4(void)
{
    Display_SpawnWithOtSmall(0, 0x26, 0, 0);
}

void func_800A99E0(Task* task)
{
    DisplayState* ds;
    s32           flag;

    ds            = &Display_State;
    flag          = (s8)ds->field_122;
    ds->field_103 = 2;
    if (flag == 0) {
        Gpu_ResetGraphAndOt();
        Tmd_AllocMissingBuffers();
    }
    func_800ACAA8();
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9A40);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9B3C);

void func_800A9BE4(void)
{
    u8  param1[8];
    u8  param2[8];
    u8  val;
    s32 flag;

    val = D_80073BA9;
    if (val == 0) {
        val = 1;
    }
    flag      = 1;
    param1[0] = val;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 3;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = flag;
    func_800A9310();
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9C50);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9CBC);

void func_800A9DF0(Task* task)
{
    TmdObject* obj;

    obj = task->extra;
    if (task->spawnType == 1) {
        obj->field_24 = 4;
        obj->field_25 = 6;
        if (obj->field_18 != NULL) {
            Tmd_ProcessStream(obj);
            Tmd_ProcessStream(obj);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9E44);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA120);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA548);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AAA68);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AABB0);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AADDC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AAF70);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB1C8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB3A8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB5F4);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB828);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB980);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABA4C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABCC8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABE68);

void func_800ABEF8(s32 arg0)
{
    GpFlagBank* bank;

    bank          = D_80060A30[arg0];
    bank->field_4 = 0;
    bank->field_8 = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABF1C);

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC008);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC058);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC0F0);

void func_800AC164(Task* task)
{
    if (CdCmd_Queue.field_224 == 0) {
        Gpu_ClearOTag(0);
        Gpu_ClearOTag(1);
        Pad_RemapState->field_3 = 0;
        Task_Kill(task);
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x1050000) {
            func_800AA548(1);
        } else {
            func_800AA548(0);
        }
        Display_State.field_11d = 0;
        Display_State.field_1d &= 0x7F;
        Display_AcquireRef();
        Task_Spawn(0, 0x21, 0, 0);
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x1050000) {
            Task_SpawnFromTable(D_80183824, 0, 0, 0);
            CdCmd_SetupMdecBuffers();
            CdCmd_SelectMdecBuffer();
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC25C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC344);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC464);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC4D8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC688);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC790);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC960);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACAA8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACD2C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACEBC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACF8C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD024);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD284);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD2E8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD378);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD410);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD50C);

void func_800AD58C(Task* task)
{
    func_800ACAA8();
    Task_Kill(task);
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD5B8);

void func_800AD620(Task* task)
{
    s32 val;

    val = func_800ACF8C();
    do {
        D_80071068 = val;
    } while (0);
    task->state++;
}

void func_800AD65C(Task* task)
{
    DisplayState* ds;
    s32           val;

    ds = &Display_State;
    if ((ds->field_1e != 2) && (ds->field_104 == 0)) {
        func_800AC688();
    } else {
        val        = func_800ACF8C();
        D_80071068 = val;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD6BC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ADA04);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ADE74);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ADF3C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE150);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE1F0);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE36C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE45C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE53C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE62C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE7AC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE9B0);
