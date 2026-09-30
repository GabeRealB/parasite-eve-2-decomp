#include "title/title.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// Title-screen work block stored at Task::work (memCalloc 0x18).
typedef struct _TitleWork {
    /* 0x00 */ s32 timer;          // frame / phase counter
    /* 0x04 */ s32 selection;      // menu cursor index
    /* 0x08 */ s32 fadeTileEnable; // fullscreen fade TILE when non-zero
    /* 0x0C */ s32 logoFade;       // intro logo alpha 0..0x80
    /* 0x10 */ s32 menuFade;       // menu chrome alpha 0..0x80
    /* 0x14 */ s32 menuCount;      // number of menu entries
} TitleWork;
STATIC_ASSERT_SIZEOF(TitleWork, 0x18);

/// Retained text labels for the title menu (src/title/title.c).
extern char Title_StrNewGame[];

extern char Title_StrLoadGame[];

extern char Title_StrConfiguration[];

extern char Title_StrDebugOption[];

extern char Title_StrExtraGame[];

extern char Title_StrSurvival[];

/// Task spawn ids for menu selection indices.
extern s32 Title_MenuSpawnIds[];

/// Last rand() from Title_Dispatch.
extern s32 Title_LastRand;

/// When set, Title_BootTask spawns phase task with arg 0x80000000 (skip fade TILE).
extern u16 Title_SkipFadeFlag;

void Title_DemoStreamTask(Task* task);

void Title_BootTask(Task* task);

void func_807246B4(void);

static void Title_FlagAdvanceTask(Task* arg0);
static void Title_InitTask(Task* arg0);
static void Title_MenuTask(Task* task);

char Title_StrNewGame[]       = "New Game";
char Title_StrLoadGame[]      = "Load Game";
char Title_StrConfiguration[] = "Configuration";
char Title_StrDebugOption[]   = "Debug Option";
char Title_StrExtraGame[]     = "Extra Game";
char Title_StrSurvival[]      = "Survival";

/// Retained menu labels in selection order. The retail menu draws its labels
/// from the texture atlas instead of reading this table.
static char* Title_MenuLabels[] = {
    Title_StrSurvival,
    Title_StrExtraGame,
    Title_StrNewGame,
    Title_StrLoadGame,
    Title_StrConfiguration,
    Title_StrDebugOption,
};

s32 Title_MenuSpawnIds[] = { 6, 6, 3, 4, 5, 6 };

TaskDesc Title_TaskDescs[] = {
    { 0, 0xC0, Title_BootTask },
    { 0, 0xC0, Title_DemoStreamTask },
};

/// Overlay state is stored in the loaded image; the loader does not clear BSS.
s32 Title_LastRand     = 0;
u16 Title_SkipFadeFlag = 0;

/// The title task's states, which `Title_Dispatch` copies and indexes by
/// `Task::state`: set-up, the flag advance, the menu in two states and the kill.
static const TaskFuncTable5 Title_PhaseTable = {
    .funcs = {
        Title_InitTask,
        Title_FlagAdvanceTask,
        Title_MenuTask,
        Title_MenuTask,
        taskKill,
    },
};

/// Debug line printed when the title screen hands over to the attract demo.
static const char Title_DemoStartMsg[] = "##########DEMO START\n";

/// Debug line printed before and after a demo restores its save data, with
/// the stage and scene it names. The two bytes after the terminator are never
/// read.
static const char Title_DemoCardRestoreMsg[44] = "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n\0\x22\xE1";

static void Title_DrawSpriteRow(s32 y, s32 v, s32 color);

static void Title_InitTask(Task* arg0)
{
    s32           flag;
    DisplayState* ds;
    TitleWork*    work;

    flag                          = 1;
    ds                            = &gDisplayState;
    ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    Wip_UiHolder                  = NULL;
    if (arg0->spawnArg1.value < 0) {
        flag                   = 0;
        arg0->spawnArg1.value &= 0x7FFFFFFF;
    }
    if (arg0->spawnArg1.value > 0) {
        arg0->spawnArg1.value -= 1;
        return;
    }
    work = memCalloc(0x18, 0);
    if (work != NULL) {
        arg0->work           = work;
        work->fadeTileEnable = flag;
        work->menuCount      = 5;
        work->selection      = 2;
        work->timer          = 0;
        if (Wip_SysFlags.field_1 != 0) {
            work->selection = 3;
        }
        Text_LoadClutImages();
        Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_KEEP_VIEW);
        ds->holdState                 = DISPLAY_HOLD_INITIAL;
        work->timer                   = -0x10;
        ds->control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (ds->debugMode != 0) {
            func_807246B4();
        }
        CdCmd_EnqueueLoadFile(1, 0, 0);
        arg0->state += 2;
        Title_MenuTask(arg0);
    }
}

/// One 16px chrome row. v is atlas Y in pe2img_2 (0 logo, 0x10 footer,
/// 0x20 cursor, 0x30+ menu). clut 0x3FC0, tpage 0xE10002BC.
static void Title_DrawSpriteRow(s32 y, s32 v, s32 color)
{
    SPRT*     p;
    DR_TPAGE* dr;
    u8        c;

    c                              = color;
    p                              = gGpuPrimCursor;
    gGpuPrimCursor                 = p + 1;
    p->x0                          = -0x80;
    p->w                           = 0x100;
    p->h                           = 0x10;
    p->clut                        = 0x3FC0;
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = (c << 16) | (c << 8) | c;
    setlen(p, 4);
    p->u0 = 0;
    p->v0 = v;
    setcode(p, 0x66);
    p->y0 = y;
    addPrim(gGpuCurrentOt, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE10002BC;
    addPrim(gGpuCurrentOt, dr);
}

static void Title_MenuTask(Task* task)
{
    TitleWork* work = (TitleWork*)task->work;
    s32        timer;
    s32        i;

    timer       = work->timer + 1;
    work->timer = timer;
    if (timer >= 0x385) {
        if (timer < 0x394) {
            if (work->fadeTileEnable != 0) {
                TILE*     tile;
                DR_TPAGE* tpage;

                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                setlen(tile, 3);
                setcode(tile, 0x60);
                tile->r0 = tile->g0 = tile->b0 = (timer - 0x384) * 16 - 1;
                tile->x0                       = -0xA0;
                tile->y0                       = -0x78;
                tile->w                        = 0x140;
                tile->h                        = 0xF0;
                setSemiTrans(tile, 1);
                addPrim(gGpuCurrentOt, tile);

                tpage          = gGpuPrimCursor;
                gGpuPrimCursor = tpage + 1;
                setlen(tpage, 1);
                tpage->code[0] = 0xE1000240;
                addPrim(gGpuCurrentOt, tpage);
            }
        } else {
            Wip_SysFlags.field_4 = 0;
            if (Wip_SysFlags.field_0 == 1) {
                Task_CallExit(task);
                gDisplayState.demoScene = GameMain_GetResetCount() + 2;
                gDisplayState.demoScene = gDisplayState.demoScene % 3 + 1;
                printf(Title_DemoStartMsg);
                Task_Spawn(0, 3, 2, 0);
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            } else {
                gDisplayState.gameMode = DISPLAY_GAME_RESTART;
            }
        }
        return;
    }

    if (work->fadeTileEnable != 0 && timer < 0) {
        TILE*     tile;
        DR_TPAGE* tpage;
        s32       color;

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setlen(tile, 3);
        setcode(tile, 0x62);
        color    = ~(work->timer << 4);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
        addPrim(gGpuCurrentOt, tile);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000240;
        addPrim(gGpuCurrentOt, tpage);
    }

    if (task->state == 3) {
        if (work->menuFade < 0x80) {
            work->menuFade += 8;
        }
        for (i = 0; i < 3; i++) {
            Title_DrawSpriteRow(i * 0xE + 0x38, i * 0x10 + 0x30, work->menuFade);
        }
        Title_DrawSpriteRow((work->selection - 2) * 0xE + 0x38, 0x20, work->menuFade);
        Title_DrawSpriteRow(work->menuFade / 8 + 0x40, 0, 0x80 - work->menuFade);
        Title_DrawSpriteRow(0x5C, 0x10, 0x80 - work->menuFade);
        if (work->menuFade < 0x80) {
            return;
        }

        if (Pad_CheckButtons(0, 1, 0x4000) != 0) {
            work->timer = 0;
            work->selection++;
            SndEvt_EnqueueType6(2, 0, 0);
            if (work->selection >= work->menuCount) {
                work->selection -= work->menuCount;
            }
            if (work->selection == 0) {
                work->selection = 1;
            }
            if (work->selection == 1) {
                work->selection = 2;
            }
        } else if (Pad_CheckButtons(0, 1, 0x1000) != 0) {
            work->timer = 0;
            work->selection--;
            SndEvt_EnqueueType6(2, 0, 0);
            if (work->selection == 1) {
                work->selection = 0;
            }
            if (work->selection == 0) {
                work->selection = -1;
            }
            if (work->selection < 0) {
                work->selection += work->menuCount;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | 0x800) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Task_Spawn(0, Title_MenuSpawnIds[work->selection], 0, 0);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            Task_CallExit(task);
        }
    } else {
        if (work->logoFade < 0x80) {
            work->logoFade += 8;
        }
        Title_DrawSpriteRow(0x40 - (0x80 - work->logoFade) / 8, 0, work->logoFade);
        Title_DrawSpriteRow(0x5C, 0x10, 0x80);
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | 0x800) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            work->timer = 0;
            task->state++;
        }
    }
}

/// Restore demo card / save banks from Fs_ActorLoadBase2 (or 0x80600100 when
/// gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY).
/// Preserves Mc_SaveData[0].state.vibration / field_23 across the bulk copy.
void Title_RestoreDemoCard(void)
{
    u8* src;
    s32 saveField23;
    s32 saveField21;
    s32 bank;
    s32 t;
    u8* base;

    src         = (u8*)Fs_ActorLoadBase2;
    bank        = GAME_FLAG_NIBBLE_BANK_LIVE;
    saveField23 = Mc_SaveData[0].state.demoScene;
    saveField21 = Mc_SaveData[0].state.vibration;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        src = (u8*)0x80600100;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData[0].state.at4.loc.stage, Mc_SaveData[0].state.at4.loc.area);

    memcpy(&Mc_SaveData[0], src, sizeof(McSaveData));
    src += sizeof(McSaveData);

    /* The save's player block is the first half of `Player_Status`, banked at
       a 0x40-byte stride; the original computes that stride, so neither
       `&Player_Status` alone nor a whole-struct stride reproduces it. */
    memcpy((u8*)&Player_Status + bank * 0x40, src, 0x40);
    src += 0x40;

    memcpy(&GameFlag_AcropolisBanks[bank], src, 0x6C);
    src += 0x6C;

    memcpy(GameFlag_DryfieldBanks, src, 0xB0);
    src += 0xB0;

    memcpy(GameFlag_DryfieldFullBanks, src, 0x24);
    src += 0x24;

    /* bank * 0xE4, split so GCC interleaves lui of GameFlag_ShelterBanks after first sll */
    t    = bank * 8;
    base = (u8*)GameFlag_ShelterBanks;
    memcpy(base + ((t - bank) * 8 + bank) * 4, src, 0xE4);
    src += 0xE4;

    memcpy(GameFlag_NeoArkBanks, src, 0xA4);
    src += 0xA4;

    memcpy(&gGameFlagNibbleBanks[bank], src, sizeof(gGameFlagNibbleBanks[bank]));

    Mc_SaveData[0].state.demoScene = saveField23;
    Mc_SaveData[0].state.vibration = saveField21;
    if (Fs_StageCdfIsAvailable(Mc_SaveData[0].state.at4.loc.stage) != 1) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData[0].state.at4.loc.stage, Mc_SaveData[0].state.at4.loc.area);
}

static void Title_FlagAdvanceTask(Task* arg0)
{
    s32* p = &arg0->state;

    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    (*p)++;
}

void Title_Dispatch(Task* arg0)
{
    TaskFuncTable5 sp;

    sp             = Title_PhaseTable;
    Title_LastRand = rand();
    sp.funcs[arg0->state](arg0);
}

void Title_ExitTask(Task* arg0)
{
    Task_CallExit(arg0);
}

void Title_DemoStreamTask(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    u8          param1[4];
    u8          param2[4];
    CdCmdQueue* queue = &CdCmd_Queue;

    switch (task->state) {
        case 0:
            Mem_CopyUnaligned(Fs_Streams, Stream_Slots, sizeof(Fs_Streams));
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key = gGameSession->at4;
            if (Wip_SysFlags.field_0 == 2) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (CdCmd_IsIdle()) {
                task->state++;
            } else if (Pad_CheckFlag800()) {
                Title_SkipFadeFlag = 0;
                SetDispMask(0);
                CdCmd_ActivatePhase1();
                task->state++;
            }
            break;
        case 4:
            if (CdCmd_IsIdle()) {
                CdCmd_Queue.preserveDisplayAfterDecode = 1;
                param1[3]                              = 0;
                param1[2]                              = 0;
                param1[0]                              = 2;
                param2[0]                              = 0;
                param2[1]                              = 0;
                param2[2]                              = 0;
                param2[3]                              = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                task->state++;
            }
            break;
        case 5:
            if (CdCmd_IsIdle()) {
                Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                task->state++;
            }
            break;
        case 6:
            Stream_ResetRestoreState();
            Display_LoadImageStrips(gDisplayState.drawBuffer);
            Display_LoadImageStrips(gDisplayState.drawBuffer ^ 1);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            task->state++;
            break;
        case 7:
            if (Stream_RestoreAfterLoad(0, 0)) {
                taskKill(task);
                Display_ResetHeapWrapper();
            }
            break;
    }
}

void Title_BootTask(Task* arg0)
{
    u8    param1[4];
    u8    param2[4];
    s32   next;
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            Title_SkipFadeFlag                      = 1;
            if ((gDisplayState.debugMode < 0) || (Wip_SysFlags.field_4 != 0)) {
                next               = 6;
                Title_SkipFadeFlag = 0;
            } else {
                Display_SpawnWithOt(Title_TaskDescs, 1, 0, 0);
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                next                                 = task->state + 1;
            }
            task->state = next;
            return;
        case 1:
        case 2:
            task->state = task->state + 1;
            return;
        case 3:
            if (Title_SkipFadeFlag != 0) {
                Task_Spawn(0, 2, 0x80000000, 0);
            } else {
                Task_Spawn(0, 2, 0, 0);
            }
            /* fallthrough */
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SetDispMask(1);
            Wip_SysFlags.field_4 = 1;
            taskKill(task);
            return;
        case 6:
            param1[3] = 0;
            param1[2] = 0;
            param1[0] = 2;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
            task->state = task->state + 1;
            /* fallthrough */
        case 7:
            if (CdCmd_IsIdle() & 0xFFFF) {
                task->state = 3;
            }
            return;
    }
}

void Title_EnqueueDemoScene(s32 arg0)
{
    u8  param2[4];
    u8* param1;

    param1                 = SCRATCH_PUSH_BYTES(8);
    gGameSession->field_80 = 0;
    param1[3]              = 0;
    param1[2]              = 0x50;
    param1[0]              = 0;
    param2[0]              = arg0 + 0xA;
    param2[3]              = 0;
    param2[2]              = 0;
    param2[1]              = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    SCRATCH_POP_BYTES(8);
}
