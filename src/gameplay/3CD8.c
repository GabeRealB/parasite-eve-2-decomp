#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/wipsys.h"

extern TaskFuncTable3 D_800974C8;
extern TaskFuncTable3 D_8009752C;
extern TaskFuncTable3 D_80097538;
extern TaskFuncTable3 D_80097678;
extern TaskDesc       D_8010FAEC[];
extern s32            D_8010FB90[];
extern u16            D_80112D68[];
extern u16            D_80113360[];
extern GpEvt12*       D_801155A8;
extern u16            D_801155AE;
extern u8             D_80115670;
extern Task*          D_80115674;
extern s16            D_80115678;
extern u8             D_80115648;
extern s16            D_80115654;
extern s16            D_80115656;
extern u8             D_80115658;
extern u8             D_80115659;
extern u8             D_8011565A;
extern s32            D_80115660;
extern s16            D_80115666;
extern s16            D_80115668;
extern s32            D_8011568C;
extern s16            D_80115698;
extern s16            D_8011569A;
extern u8             D_8011569C;
extern s32*           D_801156A0;
extern u8             D_801156A4;
extern s32            D_801156A8;
extern s8             D_801156B0;
extern s8             D_801156B1;
extern s32            D_801156B4;
extern Task*          D_801156B8;
extern s16            D_801156BC;
extern u8             D_801156F9;
extern u8             D_80115700;
extern u8             D_80115701;
extern u8             D_80115702;

s32  func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800E40EC(s32 arg0);
s32  func_800E41F4(s32 arg0, s16 arg1, s16 arg2);
void func_800E44A0(Task* arg0);
void func_80724120(void);
void func_80724324(void);
void func_80724714(void);
void func_800E646C(Task* arg0);
s32  func_800E6C70(s16 arg0, s16 arg1, s16 arg2);
s32  func_800E6CF0(void);
void func_800E6D60(s32 arg0);
void func_800E6E50(void);
s32  func_800E6EA0(s32 arg0);
s32  func_800E86FC(s32 arg0);
void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
void func_800E8A90(Task* task);
void func_800E8BB0(Task* task);
void func_800EA478(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

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

void func_800E3C6C(s32 arg0, s32 arg1)
{
    s32 out;

    arg0 = (arg0 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6;
    func_800AC464(Game_GetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    if (out != 0) {
        func_800AC464((Task*)out, 0x7D5, arg1, 0);
    }
}

void func_800E3CEC(s32* arg0)
{
    *arg0 = D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
}

void func_800E3D24(s32* arg0)
{
    *arg0 = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
}

void func_800E3D5C(void)
{
    WipSysConfig* p;

    p           = &Wip_SysConfig;
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
        Task_SpawnFromTable(D_8010FAEC, 0, arg1, arg0);
    }
}

void func_800E3DD8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    SndEvt_EnqueueType6(arg0, (s8)arg1, (s8)arg2);
}

s32 func_800E3E30(s32 arg0)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    return arg0;
}

void func_800E3E64(s32 arg0, s32 arg1)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    SndEvt_EnqueueType7(arg0, arg1 & 0xFFFF);
}

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

void func_800E4028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800974C8;
    sp.funcs[arg0->state](arg0);
}

void func_800E4080(void)
{
    s32 i;

    for (i = 0; i < 0x1F8; i++) {
        GameFlag_SetNibble(i, 0);
    }
}

void func_800E40BC(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_8010FAEC, 1, arg0, arg1);
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

s32 func_800E6C70(s16 arg0, s16 arg1, s16 arg2)
{
    s32 entry;

    if (D_801155A8 != 0) {
        return 0;
    }

    entry = D_801156A0[arg0];
    if (entry == 0) {
        return 1;
    }
    return (s16)func_800E41F4(entry, arg1, arg2);
}

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

void func_800E6D60(s32 arg0)
{
    s32           i;
    s32           count;
    register s32  type3 asm("v0");
    s32*          out;
    FsFolderSlot* slot;
    DisplayState* ds;

    count = 0;
    i     = count;
    ds    = &Display_State;
    for (; i < 50; i++) {
        out   = &D_8011568C;
        slot  = &D_8006C338[i];
        type3 = 3;
        if (slot->field_0 == type3) {
            if (count == arg0) {
                if (ds->field_112 != 0) {
                    func_80724714();
                }
                D_8011568C = slot->field_4;
                func_800E40EC(D_8011568C);
                break;
            }
            count++;
        }
    }
}

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

void func_800E6E50(void)
{
    GpEvt12* p;
    u8       field4;
    s32      base;
    s32      idx;

    idx        = *(s16*)&D_801155AE;
    base       = (s32)D_801155A8;
    p          = (GpEvt12*)(idx * sizeof(GpEvt12) + base);
    field4     = p->field_4;
    D_80115670 = field4;
    if (p->field_7 != 0) {
        D_80115670 = field4 & 0xFE;
    }
    D_80115678 = p->field_7;
}

s32 func_800E6EA0(s32 arg0)
{
    s32      flag;
    s32      id;
    s32      base;
    GpEvt12* p;

    flag = -1;
    id   = D_80115668;
    base = (s32)D_801155A8;
    p    = (GpEvt12*)(arg0 * sizeof(GpEvt12) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_800E6EF4(Task* task)
{
    if (task->state > 0) {
        if (D_801155A8 != 0 && D_8011565A == 0) {
            D_801155A8 = 0;
        }
        Task_Kill(task);
    }
    task->state++;
}

void func_800E6F60(Task* task)
{
    register s32 val asm("s0");
    GpSpawnArg*  arg;
    s32          mode;
    Task*        slot;

    switch (task->state) {
        case 0:
            arg                 = (GpSpawnArg*)&task->spawnArg1;
            task->killCountdown = arg->field_1;
            task->state++;
            break;
        case 1:
            if (task->killCountdown == 0) {
                arg  = (GpSpawnArg*)&task->spawnArg1;
                mode = arg->field_2;
                val  = arg->field_0;
                if (mode == 0) {
                    func_800AC464(Game_GetPtrSlot(3), 0x401, val, 0);
                } else if (mode == 1) {
                    slot = Game_GetPtrSlot(0xA);
                    if (slot != NULL) {
                        func_800AC464(slot, 0x401, val, 0);
                    }
                } else {
                    slot = (Task*)func_800E86FC(mode - 2);
                    if (slot != NULL) {
                        func_800AC464(slot, 0x7E0, val, 0);
                    }
                }
                Task_Kill(task);
            }
            task->killCountdown--;
            break;
    }
}

void func_800E704C(void)
{
    D_801155AE++;
    D_801155AE = func_800E6EA0((s16)D_801155AE);
    D_80115648 = 0;
    D_80115658 = 0x1E;
    D_80115659 = 0xF;
    func_800E6E50();
}

void func_800E70AC(Task* task)
{
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                if (D_80115666 == 2) {
                    func_800AC464(Game_GetPtrSlot(5), 0xBB8, 1, 0);
                }
                task->state++;
                break;
        }
        func_800E44A0(task);
    }
}

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
                Stage_SetEndingFlag();
                Task_Kill(task);
            }
            break;
    }
}

void func_800E71B0(Task* task)
{
    void* mem;

    mem = Mem_Calloc(4, 0);
    if (mem == NULL) {
        Task_Kill(task);
        return;
    }
    func_800E6DF4();
    D_801156B8     = NULL;
    task->field_24 = D_8010FB90;
    Game_SetPtrSlot(task, 6);
    task->idMap = mem;
    D_801156B0  = 0;
    task->state++;
}

void func_800E7240(void)
{
    if (Display_State.field_112 != 0) {
        func_80724120();
        func_80724324();
    }
    if (D_8011568C != 0) {
        func_800E40EC(D_8011568C);
    }
    if (func_800E6CE0() != 0 && D_801156B0 != 0) {
        D_801156BC++;
        if ((D_801156A4 & 0x20) == 0) {
            if (D_801156BC >= 0x1E) {
                D_801156A4 |= 0x20;
                D_801156B0  = 0;
            }
        }
    }
}

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

s32 func_800E7378(void)
{
    if (Mc_SaveData.field_23 == 9) {
        if (D_801156B8 != NULL) {
            return 0;
        }
        D_801156B8 = Task_Spawn(9, 8, 0, 0);
    } else {
        Game_Session->field_68 = 1;
    }
    return 0;
}

s32 func_800E73E8(void)
{
    Task* task;

    if (Mc_SaveData.field_23 == 9) {
        task = D_801156B8;
        if (task != NULL) {
            task->spawnArg1 = 1;
            D_801156B8      = NULL;
            return 0;
        }
    } else {
        Game_Session->field_68 = 0;
    }
    return 0;
}

s32 func_800E7434(void)
{
    if (Mc_SaveData.field_23 == 9) {
        if (D_801156B8 == NULL) {
            return 0;
        }
        Task_Kill(D_801156B8);
        D_801156B8 = NULL;
    } else {
        Game_Session->field_68 = 0;
    }
    return 0;
}

s32 func_800E7498(s32 arg0, s32 arg1, GpOverlayIds* arg2)
{
    if (arg2 != NULL) {
        CdCmd_StartOverlay(arg2->field_0, arg2->field_2, arg2->field_4);
    }
    D_801156B4 = 1;
    D_801156B1 = arg2 != NULL;
    return 0;
}

s32 func_800E74EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (Game_Session->field_5F == 0) {
        if (D_801156B1 != 0) {
            func_8001D580();
            D_801156B0 = 1;
            D_801156BC = 0;
        } else {
            D_801156B0 = 1;
            if (arg2 == 2) {
                D_801156BC = 0x1E;
            } else {
                D_801156BC = 0;
            }
        }
    }
    return 0;
}

void func_800E7570(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_8009752C;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E75C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8378);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E84B8);

void func_800E8614(s32 arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8634);

s32 func_800E86FC(s32 arg0)
{
    s32 out;

    arg0 = (arg0 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6;
    func_800AC464(Game_GetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    return out;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E8758);

void func_800E8830(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097538;
    sp.funcs[arg0->state](arg0);
}

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

void func_800E8F68(void)
{
    D_80115700              = 1;
    D_80115701              = 1;
    D_80115702              = 1;
    Game_Session->field_13B = 0;
    Pad_ClearEvents(0);
}

Task* func_800E8FB0(s32 arg0, s32 arg1)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = 0;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

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

Task* func_800E9188(s32 arg0, s32 arg1, s32 arg2)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = arg2;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E9218);

void func_800E92BC(void)
{
}

void func_800E92C4(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_10 == 0) {
        func_800E8A90(task);
    }
}

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

void func_800EA3B4(void)
{
    s32        i;
    GpCoord64* p;

    p = D_80114F30;
    for (i = 0; i < 8; i++) {
        if (p->field_0 != 0) {
            p->field_0--;
        }
        p++;
    }
}

void func_800EA3EC(void)
{
    s32        i;
    GpCoord64* p;

    p = D_80114F30;
    for (i = 0; i < 8; i++) {
        p->coord.sub = &D_80070F10;
        p->field_0   = 0;
        p++;
    }
}

void func_800EA420(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097678;
    sp.funcs[arg0->state](arg0);
}

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

void func_800EC9C8(void)
{
    if (!(D_80115740->field_10 & 1)) {
        func_800EA478(0x800600E8, 0, 0, 0);
    }
}

void func_800ECA10(s32 arg0)
{
    D_80115740->field_18 = (u8)arg0;
    func_800EA478(0x8006000F, 0, (u8)arg0, 0);
}

void func_800ECA54(void)
{
    GpState1C* p;

    p = D_80115740;
    if (!(p->field_10 & 0x80)) {
        p->field_12 &= 0xF7FF;
        func_800EA478(0x8006000E, 0, 0, 0);
    }
}
