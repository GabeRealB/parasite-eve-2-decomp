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
#include "main/text.h"

/* Define BSS before API headers to preserve first-declaration order. */
s8 D_801156B0;

s8 D_801156B1;

s32 D_801156B4;

Task* D_801156B8;

s16 D_801156BC;

#include "captions.h"

extern TaskMessageEntry D_8010FB90[10];

s32 Gp_StartCapAndClear(Task* arg0, s32 arg1, s16 arg2, s32 arg3);

s32 func_800E731C(Task*, s32, s32, s32);

s32 Gp_AbortCapClear(Task*, s32, s32, s32);

s32 func_800E7358(Task*, s32, s32, s32);

s32 func_800E7378(Task*, s32, s32, s32);

s32 func_800E73E8(Task*, s32, s32, s32);

s32 func_800E7434(Task*, s32, s32, s32);

s32 func_800E7498(Task* arg0, s32 arg1, EvsSceneKey* sceneKey, s32 arg3);

s32 func_800E74EC(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_80724120(void);

void func_80724324(void);

TaskMessageEntry D_8010FB90[10] = {
    { CAP_CONTROL_MESSAGE_START, Gp_StartCapAndClear },
    { 0xFA1, func_800E731C },
    { CAP_CONTROL_MESSAGE_ABORT, Gp_AbortCapClear },
    { CAP_CONTROL_MESSAGE_IS_BUSY, func_800E7358 },
    { CAP_CONTROL_MESSAGE_HIDE_HUD, func_800E7378 },
    { CAP_CONTROL_MESSAGE_SHOW_HUD, func_800E73E8 },
    { CAP_CONTROL_MESSAGE_SHOW_HUD_ABORT, func_800E7434 },
    { 0xFA6, func_800E7498 },
    { 0xFA7, func_800E74EC },
    { -1, NULL },
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
    gameSetTaskSlot(task, GAME_TASK_SLOT_CAP_CONTROL);
    task->work = mem;
    D_801156B0 = 0;
    task->state++;
}

void Gp_CapTaskState1(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        func_80724120();
        func_80724324();
    }
    if (Gp_CapFile != 0) {
        capRelocateFile(Gp_CapFile);
    }
    if (capIsBusy() != 0 && D_801156B0 != 0) {
        D_801156BC++;
        if ((D_801156A4 & 0x20) == 0) {
            if (D_801156BC >= 0x1E) {
                D_801156A4 |= 0x20;
                D_801156B0  = 0;
            }
        }
    }
}

s32 Gp_StartCapAndClear(Task* arg0, s32 arg1, s16 arg2, s32 arg3)
{
    Gp_StartCapSlot(arg2, 0, 0);
    D_801156B0 = 0;
    return 0;
}

s32 func_800E731C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_8011569A = 0;
    D_80115698 = 0;
    return 0;
}

s32 Gp_AbortCapClear(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_801156B0 = 0;
    return Gp_AbortCap();
}

s32 func_800E7358(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return capIsBusy();
}

s32 func_800E7378(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
        if (D_801156B8 != NULL) {
            return 0;
        }
        D_801156B8 = taskSpawn(9, 8, 0, 0);
    } else {
        gGameSession->hideHud = 1;
    }
    return 0;
}

s32 func_800E73E8(Task* msgTask, s32 msgId, s32 arg2, s32 arg3)
{
    Task* task;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
        task = D_801156B8;
        if (task != NULL) {
            task->spawnArg1.value = 1;
            D_801156B8            = NULL;
            return 0;
        }
    } else {
        gGameSession->hideHud = 0;
    }
    return 0;
}

s32 func_800E7434(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
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

s32 func_800E7498(Task* arg0, s32 arg1, EvsSceneKey* sceneKey, s32 arg3)
{
    if (sceneKey != NULL) {
        CdCmd_StartOverlay(sceneKey->group, sceneKey->streamId, sceneKey->subId);
    }
    D_801156B4 = 1;
    D_801156B1 = sceneKey != NULL;
    return 0;
}

s32 func_800E74EC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (gGameSession->evtSkipped == 0) {
        if (D_801156B1 != 0) {
            cdCmdSceneControlNoOp();
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
