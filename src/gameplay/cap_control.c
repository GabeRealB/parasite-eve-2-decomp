#include "gameplay/captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "evs.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

/* Define BSS before API headers to preserve first-declaration order. */
s8 D_801156B0;

s8 D_801156B1;

s32 D_801156B4;

Task* D_801156B8;

s16 D_801156BC;

#include "captions.h"

typedef struct {
    s32 id;
    union {
        s32 (*empty)(void);
        s32 (*start)(s32, s32, s16);
        s32 (*overlay)(s32, s32, GpOverlayIds*);
        s32 (*value)(s32, s32, s32);
    } handler;
} GpCapControlEntry;

extern GpCapControlEntry D_8010FB90[10];

s32 Gp_StartCapAndClear(s32 arg0, s32 arg1, s16 arg2);

s32 func_800E731C(void);

s32 Gp_AbortCapClear(void);

s32 func_800E7358(void);

s32 func_800E7378(void);

s32 func_800E73E8(void);

s32 func_800E7434(void);

s32 func_800E7498(s32 arg0, s32 arg1, GpOverlayIds* arg2);

s32 func_800E74EC(s32 arg0, s32 arg1, s32 arg2);

void func_80724120(void);

void func_80724324(void);

GpCapControlEntry D_8010FB90[10] = {
    { 0xFA0, { .start = Gp_StartCapAndClear } },
    { 0xFA1, { .empty = func_800E731C } },
    { 0xFA2, { .empty = Gp_AbortCapClear } },
    { 0xFA3, { .empty = func_800E7358 } },
    { 0xFA4, { .empty = func_800E7378 } },
    { 0xFA5, { .empty = func_800E73E8 } },
    { 0xFA8, { .empty = func_800E7434 } },
    { 0xFA6, { .overlay = func_800E7498 } },
    { 0xFA7, { .value = func_800E74EC } },
    { -1, { .empty = NULL } },
};

void Gp_InitCapTask(Task* task)
{
    void* mem;

    mem = memCalloc(4, 0);
    if (mem == NULL) {
        taskKill(task);
        return;
    }
    Gp_ResetCap();
    D_801156B8     = NULL;
    task->msgTable = D_8010FB90;
    Game_SetPtrSlot(task, 6);
    task->work = mem;
    D_801156B0 = 0;
    task->state++;
}

void Gp_CapTaskState1(void)
{
    if (gDisplayState.field_112 != 0) {
        func_80724120();
        func_80724324();
    }
    if (Gp_CapFile != 0) {
        Gp_RelocCapFile(Gp_CapFile);
    }
    if (Gp_CapBusy() != 0 && D_801156B0 != 0) {
        D_801156BC++;
        if ((D_801156A4 & 0x20) == 0) {
            if (D_801156BC >= 0x1E) {
                D_801156A4 |= 0x20;
                D_801156B0  = 0;
            }
        }
    }
}

s32 Gp_StartCapAndClear(s32 arg0, s32 arg1, s16 arg2)
{
    Gp_StartCapSlot(arg2, 0, 0);
    D_801156B0 = 0;
    return 0;
}

s32 func_800E731C(void)
{
    D_8011569A = 0;
    D_80115698 = 0;
    return 0;
}

s32 Gp_AbortCapClear(void)
{
    D_801156B0 = 0;
    return Gp_AbortCap();
}

s32 func_800E7358(void)
{
    return Gp_CapBusy();
}

s32 func_800E7378(void)
{
    if (Mc_SaveData[0].demoScene == 9) {
        if (D_801156B8 != NULL) {
            return 0;
        }
        D_801156B8 = Task_Spawn(9, 8, 0, 0);
    } else {
        gGameSession->hideHud = 1;
    }
    return 0;
}

s32 func_800E73E8(void)
{
    Task* task;

    if (Mc_SaveData[0].demoScene == 9) {
        task = D_801156B8;
        if (task != NULL) {
            task->spawnArg1 = 1;
            D_801156B8      = NULL;
            return 0;
        }
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

s32 func_800E7434(void)
{
    if (Mc_SaveData[0].demoScene == 9) {
        if (D_801156B8 == NULL) {
            return 0;
        }
        taskKill(D_801156B8);
        D_801156B8 = NULL;
    } else {
        gGameSession->hideHud = 0;
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
    if (gGameSession->evtSkipped == 0) {
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

    sp = Gp_CapTaskStates;
    sp.funcs[arg0->state](arg0);
}
