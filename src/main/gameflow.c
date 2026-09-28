#include "main/gameflow.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpad.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "gameflow.h"
#include "main/gamemain.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "mem.h"
#include "pad.h"
#include "main/pad_types.h"
#include "pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "gameplay/model_lighting.h"

#include "title/title.h"

/// Stack workspace for controller polling and analog-axis normalization.
typedef struct _PadPollWork {
    /* 0x00 */ s32 state;
    /* 0x04 */ s32 reserved;
    /* 0x08 */ s32 delta;
    /* 0x0C */ s32 range;
    /* 0x10 */ s32 port;
} PadPollWork;
STATIC_ASSERT_SIZEOF(PadPollWork, 0x14);

/* Define BSS before API headers to preserve first-declaration order. */
static GameSession D61CC0_800714C0;

/// Unreferenced.
static u8 D_80071600[0x20];

PadState Pad_States[2];

#include "main/pad.h"

/// Unreferenced.
static s32 D_8005ED6C;

/// Unreferenced.
static s32 D_8005ED7C;

/// Unreferenced.
static s32 D_8005ED80;

static u8 D_8005ED84[];

static const TaskFuncTable5 GameFlow_States5;

static const TaskFuncTable3 GameFlow_States3;

static void GameFlow_InitSystems(void);

static void Game_ResetSessionAndBuffers(Task* task);

static void GameFlow_SpawnMenu(Task* task);

static void GameFlow_WaitMenuDone(Task* task);

static void GameFlow_CountdownAdvance(Task* task);

static void GameFlow_SpawnMainWhenReady(Task* task);

static void GameFlow_CopySaveIds(Task* task);

static void GameFlow_EnqueueDefaultLoad(Task* task);

static void GameFlow_SpawnWhenIdle(Task* task);

static void Pad_TickEventBanks(PadState* pad);

GameSession* gGameSession = &D61CC0_800714C0;
s32          D_8005ED68   = 0;
/// Unreferenced.
static s32 D_8005ED6C      = 0x40;
s32        Pad_MaskConfirm = 0x40;
s32        Pad_MaskCancel  = 0xA0;
s32        Pad_MaskMenu    = 0x900;
/// Unreferenced.
static s32 D_8005ED7C = 0x10;
/// Unreferenced.
static s32 D_8005ED80   = 0x80;
static u8  D_8005ED84[] = { 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
u16        D_8005ED8A   = 0;

static const TaskFuncTable5 GameFlow_States5 = { {
    Game_ResetSessionAndBuffers,
    GameFlow_SpawnMenu,
    GameFlow_WaitMenuDone,
    GameFlow_CountdownAdvance,
    GameFlow_SpawnMainWhenReady,
} };

static const TaskFuncTable3 GameFlow_States3 = { {
    GameFlow_CopySaveIds,
    GameFlow_EnqueueDefaultLoad,
    GameFlow_SpawnWhenIdle,
} };

void GameFlow_StateByField34(Task* task)
{
    CdCmdQueue* p;
    s32         saved;

    p = &CdCmd_Queue;
    if (task->spawnArg1.value == 2) {
        if (task->state == 0) {
            Pad_SetCooldown(0);
            if (gDisplayState.demoScene == 0) {
                gDisplayState.demoScene = 1;
            }
            if (gDisplayState.demoScene < 0x10) {
                Title_EnqueueDemoScene(gDisplayState.demoScene - 1);
            }
            task->state = task->state + 1;
        }
        if (CdCmd_IsIdle() != 0) {
            if (gDisplayState.roomVariant == 0) {
                gDisplayState.roomVariant = 1;
            }
            Title_RestoreDemoCard();
            MEM_CLEAR(gGameSession, sizeof(GameSession));
            gDisplayState.at100.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                  = 0;
            gGameSession->applySavePlace               = 1;
            gGameSession->field_80                     = 0;
            Snd_SetMutedVolumes(1);
            gDisplayState.at100.flags.pendingPlayerPos = 0;
            gDisplayState.stopTaskWalk                 = 1;
            taskKill(task);
            Task_ResetDefaultList();
            Tmd_InitLists();
            Mem_Init();
            Task_Spawn(0, 9, 0, 0);
        }
    } else {
        gDisplayState.demoScene = 0;
        Pad_SetCooldown(0);
        if (task->spawnArg1.value == 0) {
            saved = Mc_SaveData[0].state.vibration;
            MEM_CLEAR(gGameSession, sizeof(GameSession));
            gDisplayState.at100.flags.pendingPlayerPos = 0;
            gDisplayState.gameRunning                  = 1;
            p->field_248                               = 1;
            p->field_244                               = 1;
            Wip_SysFlags.field_4                       = 1;
            Mc_InitBufferSlots();
            Mc_SaveData[0].state.vibration = saved;
            task->state                    = task->state + 1;
        } else {
            MEM_CLEAR(gGameSession, sizeof(GameSession));
            gDisplayState.gameRunning                  = 1;
            gDisplayState.at100.flags.pendingPlayerPos = 0;
            p->field_248                               = 1;
            p->field_244                               = 1;
            Wip_SysFlags.field_4                       = 1;
            gGameSession->applySavePlace               = 1;
        }
        gDisplayState.stopTaskWalk = 1;
        taskKill(task);
        Task_ResetDefaultList();
        Tmd_InitLists();
        Mem_Init();
        Task_Spawn(0, 9, 0, 0);
    }
}

void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode)
{
    TILE*     p;
    DR_TPAGE* dr;
    s8        yoff;

    p              = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->x0 = -0xA0;
    p->r0 = r;
    p->g0 = g;
    p->b0 = b;
    yoff  = gDisplayState.vramYOffset;
    p->w  = 0x140;
    p->h  = 0xF0;
    p->y0 = -0x78 - yoff;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr             = (DR_TPAGE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, (mode & 3) << 5);
    addPrim(gGpuCurrentOt - 0x10, dr);
}

void Game_ClearSession(void)
{

    MEM_CLEAR(gGameSession, sizeof(GameSession));
    gDisplayState.at100.flags.pendingPlayerPos = 0;
}

static void GameFlow_InitSystems(void)
{
    Task_ResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

static void Game_ResetSessionAndBuffers(Task* task)
{
    s32         saved;
    CdCmdQueue* p;

    p     = &CdCmd_Queue;
    saved = Mc_SaveData[0].state.vibration;
    MEM_CLEAR(gGameSession, sizeof(GameSession));
    gDisplayState.at100.flags.pendingPlayerPos = 0;
    gDisplayState.gameRunning                  = 1;
    p->field_248                               = 1;
    p->field_244                               = 1;
    Wip_SysFlags.field_4                       = 1;
    Mc_InitBufferSlots();
    do {
        Mc_SaveData[0].state.vibration = saved;
    } while (0);
    task->state = task->state + 1;
}

static void GameFlow_SpawnMenu(Task* task)
{
    void* temp_v0;

    GameMain_SetFrameTiming(0);
    temp_v0                 = Ui_SpawnFromDesc(Mc_TaskDescriptors, 0, 1, 0, 0);
    task->spawnArg2.pointer = temp_v0;
    if (temp_v0 != 0) {
        gDisplayState.gameMode = 0xFF;
        gGameSession->uiOpen   = 1;
        task->killCountdown    = 0x10;
        task->state            = task->state + 1;
    }
}

static void GameFlow_WaitMenuDone(Task* task)
{
    UiObject* obj;

    obj = task->spawnArg2.pointer;
    if (obj->field_2E == -1) {
        Ui_TeardownTree(obj, obj->owner);
        gDisplayState.gameMode = 0;
        gGameSession->uiOpen   = 0;
        if (Mc_SaveData[0].state.soundMode == 1) {
            CdVol_SetMixMode(0);
        } else {
            CdVol_SetMixMode(1);
        }
        Snd_ApplyVolumeTable(0);
        task->killCountdown = 0xC;
        task->state         = task->state + 1;
    }
}

static void GameFlow_CountdownAdvance(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown != 0) {
        return;
    }
    Pad_SetCooldown(0);
    task->state = task->state + 1;
}

static void GameFlow_SpawnMainWhenReady(Task* task)
{
    if (gDisplayState.at100.flags.pendingPlayerPos == 0) {
        Task_Spawn(0, 2, 0, 0);
        Display_SetMode(0x5010);
        taskKill(task);
        return;
    }
    gDisplayState.stopTaskWalk = 1;
    taskKill(task);
    Task_ResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void GameFlow_DispatchTable5(Task* task)
{
    TaskFuncTable5 sp;

    sp = GameFlow_States5;
    sp.funcs[task->state](task);
}

static void GameFlow_CopySaveIds(Task* task)
{
    gGameSession->at4.raw = Mc_SaveData[0].state.at4.raw;
    D_8007A394            = 0;
    task->state           = task->state + 1;
}

static void GameFlow_EnqueueDefaultLoad(Task* task)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)LoadUi_PollDiskSwap() == 0) {
        Fs_BeginBootLoad(&gGameSession->at4.loc.view, 0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        task->state = task->state + 1;
    }
}

void Game_ClearEd68(void)
{
    D_8005ED68 = 0;
}

static void GameFlow_SpawnWhenIdle(Task* task)
{
    if (CdCmd_IsIdle() != 0) {
        Task_Spawn(0, 0x11, 1, 0);
        taskKill(task);
    }
}

void GameFlow_DispatchTable(Task* task)
{
    TaskFuncTable3 sp;

    sp = GameFlow_States3;
    Pad_SetCooldown(0);
    sp.funcs[task->state](task);
}

static void Pad_TickEventBanks(PadState* pad)
{
    u8*       motor;
    PadEvent* ev;
    s32       i;

    SCRATCH_PUSH_BYTES(4);
    motor    = SCRATCH_HEAD(u8);
    motor[1] = 0;
    motor[0] = 0;

    ev = pad->events[0];
    for (i = 0; i < 8; i++, ev++) {
        if (ev->field_0 != 0) {
            if (--ev->field_2 == 0) {
                ev->field_0 = 0;
            }
            if (ev->field_1 != 0) {
                motor[0] = 1;
            }
        }
    }

    ev = pad->events[1];
    for (i = 0; i < 8; i++, ev++) {
        if (ev->field_0 != 0) {
            if (--ev->field_2 == 0) {
                ev->field_0 = 0;
            }
            if (motor[1] < ev->field_1) {
                motor[1] = ev->field_1;
            }
        }
    }

    if (Mc_SaveData[0].state.vibration == 0) {
        pad->field_5A = motor[0];
        pad->field_5B = motor[1];
    } else {
        pad->field_5A = 0;
        pad->field_5B = 0;
    }

    SCRATCH_POP_BYTES(4);
}

void Pad_PollControllers(void)
{
    PadPollWork  buffer;
    PadPollWork* work;
    PadState*    pad;
    s16*         axis;
    s16          status;
    s32          portId;
    s32          delta;
    s32          i;
    s32          modeRequested;
    s32          port;
    PadRawPort*  raw;
    u32          state;
    u32          mode;
    u32          savedState;
    u8*          rawAxis;
    u8           center;

    work = &buffer;
    port = 0;
    do {
        portId      = port * 0x10;
        pad         = &Pad_States[port];
        work->port  = portId;
        state       = PadGetState(portId);
        work->state = state;
        switch (state) {
            case 4:
                break;
            case 0:
                pad->initialized = 1;

            case 1:
                pad->unknown_58[0] = 0;
                break;
            default:
            case 2:
            case 3:
            case 5:
            case 6:
                modeRequested = 0;
                if ((pad->initialized == 1) && ((PadInfoMode(work->port, 2, 0) == 0) || (modeRequested = 1, (PadSetMainMode(work->port, 1, 0) != 0)))) {
                    pad->initialized = 0;
                }
                Pad_TickEventBanks(pad);
                if ((u8)pad->unknown_58[0] == 0) {
                    savedState = work->state;
                    if (savedState == 2) {
                        PadSetAct(work->port, &pad->field_5A, 2);
                        if (pad->field_5A != 0) {
                            pad->field_5B = 1;
                        } else {
                            pad->field_5B = 0;
                        }
                        pad->field_5A = 0x40;
                    } else if ((savedState == 6) && (modeRequested == 0)) {
                        PadSetAct(work->port, &pad->field_5A, 2);
                        if (PadSetActAlign(work->port, D_8005ED84) != 0) {
                            pad->unknown_58[0] = 1;
                        }
                    }
                }
                break;
        }
        mode = PadInfoMode(work->port, 1, 0);
        switch (mode) {
            case 5:
            case 7:
                if (pad->status != 0x73) {
                    *(u32*)pad->unknown_C = 0x80808080;
                    for (i = 0; i < 4; i++) {
                        center = (u8)pad->unknown_C[i];
                        if (center < 0x1AU) {
                            pad->unknown_C[i] = 0x1A;
                        } else if (center >= 0xE6U) {
                            *(u8*)&pad->unknown_C[i] = 0xE5;
                        }
                    }
                    pad->status = 0x73;
                }
                axis    = &pad->field_50;
                rawAxis = (u8*)Pad_RawPorts[port].unknown_4;
                i       = 0;
                do {
                    delta       = *rawAxis - (u8)pad->unknown_C[i];
                    work->delta = delta;
                    if ((u32)(delta + 0x18) < 0x31U) {
                        *axis++ = 0;
                    } else {
                        if (*rawAxis < 2U) {
                            *axis++ = -0x1000;
                        } else if (*rawAxis >= 0xFEU) {
                            *axis++ = 0x1000;
                        } else if (work->delta < 0) {
                            work->range = (u8)pad->unknown_C[i] - 0x19;
                            work->delta = (-0x18 - work->delta) << 12;
                            work->delta = work->delta / work->range;
                            *axis++     = -(s16)work->delta;
                        } else {
                            work->range = 0xE6 - (u8)pad->unknown_C[i];
                            work->delta = (work->delta - 0x18) << 12;
                            work->delta = work->delta / work->range;
                            *axis++     = (s16)work->delta;
                        }
                    }
                    i += 1;
                    rawAxis++;
                } while (i < 4);
                pad->status = 0x73;
                break;
            case 0:
            case 1:
            case 3:
            case 6:
            case 8:
                raw           = &Pad_RawPorts[port];
                raw->field_2  = 0xFF;
                raw->field_3  = 0xFF;
                status        = 0xFF;
                pad->status   = status;
                pad->field_56 = 0;
                pad->field_54 = 0;
                pad->field_52 = 0;
                pad->field_50 = 0;
                break;
            default:
                pad->status   = 0x41;
                pad->field_56 = 0;
                pad->field_54 = 0;
                pad->field_52 = 0;
                pad->field_50 = 0;
                break;
        }
        port += 1;
    } while (port <= 0);
}

void Pad_UpdatePort0(void)
{
    s32           i;
    DisplayState* ds;
    PadScratch*   scratch;
    PadState*     pad;
    u16           buttons;
    u16           prev;
    void**        head;

    head    = SCRATCH_HEAD_ADDR;
    i       = 0;
    ds      = &gDisplayState;
    scratch = SCRATCH_PUSH_AT(head, PadScratch);

    do {
        pad = &Pad_States[i];
        if (pad->cooldown == 0) {
            scratch->rawHi   = Pad_RawPorts[i].field_2;
            scratch->rawLo   = Pad_RawPorts[i].field_3;
            buttons          = ~*(u16*)&scratch->rawLo;
            scratch->buttons = buttons;

            if (pad->status == 0x73) {
                if (pad->field_54 < -0x800) {
                    scratch->buttons = buttons | 0x8000;
                }
                if (pad->field_54 >= 0x801) {
                    scratch->buttons = scratch->buttons | 0x2000;
                }
                if (pad->field_56 < -0x800) {
                    scratch->buttons = scratch->buttons | 0x1000;
                }
                if (pad->field_56 >= 0x801) {
                    scratch->buttons = scratch->buttons | 0x4000;
                }
            }

            if (i == 0) {
                if (Pad_RemapState->field_8 != 0) {
                    if (ds->displayOwner == 0) {
                        pad->field_52 = 0;
                        pad->field_50 = 0;
                        pad->field_56 = 0;
                        pad->field_54 = 0;
                        Gp_ApplyPadReplay(Pad_RemapState->field_8, scratch);
                    }
                }
            }

            prev                 = pad->buttons;
            buttons              = scratch->buttons;
            scratch->prevButtons = prev;
            pad->prevButtons     = buttons & (buttons ^ prev);
            pad->triggered       = scratch->prevButtons & (scratch->buttons ^ scratch->prevButtons);
            pad->buttons         = scratch->buttons;

            if ((s8)gGameSession->uiOpen != 0) {
                if ((scratch->prevButtons & 0xF000) == (scratch->buttons & 0xF000)) {
                    pad->autoRepeat = pad->autoRepeat + ds->frameTicks;
                } else {
                    pad->autoRepeat = 0;
                }
                if (pad->autoRepeat >= 0x1E) {
                    pad->autoRepeat  = 0x16;
                    pad->prevButtons = pad->prevButtons | (pad->buttons & 0xF000);
                }
            }
        } else {
            pad->cooldown--;
            pad->prevButtons = 0;
            pad->triggered   = 0;
            pad->buttons     = 0;
            if (pad->cooldown == 0) {
                scratch->rawHi   = Pad_RawPorts[i].field_2;
                scratch->rawLo   = Pad_RawPorts[i].field_3;
                buttons          = ~*(u16*)&scratch->rawLo;
                scratch->buttons = buttons;
                pad->buttons     = buttons;
            }
        }
        i++;
    } while (i <= 0);

    SCRATCH_POP(PadScratch);
}
