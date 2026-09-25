#include "common.h"

#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/title.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern WipUiHolder* Wip_UiHolder;
extern u8           D_800733F0[2][0x6C];
extern u8           D_800734C8[2][0xB0];
extern u8           D_80073628[2][0x24];
extern u8           D_80073670[2][0xE4];
extern u8           D_80073838[2][0xA4];
extern u8           D_80073980[0x208];
extern s32          Pad_MaskConfirm;

void func_807246B4(void);

/* Package header: title_rodata.c. This TU’s .rodata is only switch jtables. */

void Title_InitTask(Task* arg0)
{
    register s32  flag asm("s2");
    DisplayState* ds;
    TitleWork*    work;

    flag                        = 1;
    ds                          = &gDisplayState;
    ds->at100.flags.imageSource = 0;
    Wip_UiHolder                = NULL;
    if (arg0->spawnArg1 < 0) {
        flag             = 0;
        arg0->spawnArg1 &= 0x7FFFFFFF;
    }
    if (arg0->spawnArg1 > 0) {
        arg0->spawnArg1 -= 1;
        return;
    }
    work = memCalloc(0x18, 0);
    if (work != NULL) {
        arg0->work                            = (TaskIdMap*)work;
        *(volatile s32*)&work->menuCount      = 5;
        *(volatile s32*)&work->selection      = 2;
        *(volatile s32*)&work->fadeTileEnable = flag;
        *(volatile s32*)&work->timer          = 0;
        if (Wip_SysFlags.field_1 != 0) {
            work->selection = 3;
        }
        Text_LoadClutImages();
        Display_SetMode(0x9010);
        ds->holdState               = -1;
        work->timer                 = -0x10;
        ds->at100.flags.imageSource = 1;
        if (ds->field_112 != 0) {
            func_807246B4();
        }
        CdCmd_EnqueueLoadFile(1, 0, 0);
        arg0->state += 2;
        Title_MenuTask(arg0);
    }
}

void Title_DrawSpriteRow(s32 y, s32 v, s32 color)
{
    SPRT*     p;
    DR_TPAGE* dr;
    u8        c;

    c                     = color;
    p                     = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    p->x0                 = -0x80;
    p->w                  = 0x100;
    p->h                  = 0x10;
    p->clut               = 0x3FC0;
    PRIM_COLOR_WORD(p, 0) = (c << 16) | (c << 8) | c;
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

void Title_MenuTask(Task* task)
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

                tile           = (TILE*)gGpuPrimCursor;
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
                gDisplayState.at100.flags.imageSource = 0;
            } else {
                gDisplayState.gameMode = 1;
            }
        }
        return;
    }

    if (work->fadeTileEnable != 0 && timer < 0) {
        TILE*     tile;
        DR_TPAGE* tpage;
        s32       color;

        tile           = (TILE*)gGpuPrimCursor;
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
            gDisplayState.at100.flags.imageSource = 0;
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

/// Restore demo card / save banks from D_8005C374 (or 0x80600100 when
/// gDisplayState.demoScene == 0x10).
/// Preserves Mc_SaveData.vibration / field_23 across the bulk copy.
void Title_RestoreDemoCard(void)
{
    u8* src;
    s32 saveField23;
    s32 saveField21;
    s32 bank;
    s32 t;
    u8* base;

    src         = (u8*)D_8005C374;
    bank        = 0;
    saveField23 = Mc_SaveData.demoScene;
    saveField21 = Mc_SaveData.vibration;
    if (gDisplayState.demoScene == 0x10) {
        src = (u8*)0x80600100;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData.at4.loc.stage, Mc_SaveData.at4.loc.area);

    memcpy(&Mc_SaveData, src, sizeof(McSaveData));
    src += sizeof(McSaveData);

    /* The save's player block is the first half of `Player_Status`, banked at
       a 0x40-byte stride; the original computes that stride, so neither
       `&Player_Status` alone nor a whole-struct stride reproduces it. */
    memcpy((u8*)&Player_Status + bank * 0x40, src, 0x40);
    src += 0x40;

    memcpy(D_800733F0[bank], src, 0x6C);
    src += 0x6C;

    memcpy(D_800734C8, src, 0xB0);
    src += 0xB0;

    memcpy(D_80073628, src, 0x24);
    src += 0x24;

    /* bank * 0xE4, split so GCC interleaves lui of D_80073670 after first sll */
    t    = bank * 8;
    base = (u8*)D_80073670;
    memcpy(base + ((t - bank) * 8 + bank) * 4, src, 0xE4);
    src += 0xE4;

    memcpy(D_80073838, src, 0xA4);
    src += 0xA4;

    memcpy(&D_80073980[bank * 0x100], src, 0x100);

    Mc_SaveData.demoScene = saveField23;
    Mc_SaveData.vibration = saveField21;
    if (Fs_StageCdfIsAvailable(Mc_SaveData.at4.loc.stage) != 1) {
        gDisplayState.gameMode = 1;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData.at4.loc.stage, Mc_SaveData.at4.loc.area);
}

void Title_FlagAdvanceTask(Task* arg0)
{
    s32* p = &arg0->state;

    gDisplayState.at100.flags.imageSource = 1;
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

void Title_DemoStreamTask(Task* arg0)
{
    u8                     slotParam[4];
    GameLoc                key;
    u8                     param1[4];
    u8                     param2[4];
    CdCmdQueue*            queue;
    s16                    slot;
    register DisplayState* ds asm("s0");
    register Task*         task asm("s1");

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
        case 6:
            goto L_case6;
        case 7:
            goto L_case7;
    }
    return;

L_case0:
    Mem_CopyUnaligned(Fs_Streams, Stream_Slots, 0x190);
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = gGameSession->at4;
    if (Wip_SysFlags.field_0 == 2) {
        key.loc.view = 0x65;
    } else {
        key.loc.view = 0x64;
    }
    slot = Stream_FindSlot(key.raw.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    {
        register s32 mask asm("a0");
        mask = 0;
        SOFT_TOUCH_REG(mask);
        Title_SkipFadeFlag = 0;
        SetDispMask(mask);
    }
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    {
        register s32 cmd asm("a0");
        u8*          p1;
        u8*          p2;
        cmd = 0x21;
        p1  = param1;
        p2  = param2;
        SOFT_TOUCH_REG3(cmd, p1, p2);
        CdCmd_Queue.field_23E = 1;
        param1[3]             = 0;
        param1[2]             = 0;
        param1[0]             = 2;
        param2[0]             = 0;
        param2[1]             = 0;
        param2[2]             = 0;
        param2[3]             = 0;
        CdCmd_Enqueue(cmd, p1, p2);
    }
    goto advance;

L_case5:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Display_SetMode(0xD010);
    goto advance;

L_case6:
    Stream_ResetRestoreState();
    ds = &gDisplayState;
    Display_LoadImageStrips(ds->drawBuffer);
    Display_LoadImageStrips(ds->drawBuffer ^ 1);
    ds->at100.flags.imageSource = 1;
advance:
    task->state = task->state + 1;
    return;

L_case7:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

void Title_BootTask(Task* arg0)
{
    u8             param1[4];
    u8             param2[4];
    s32            next;
    register Task* task asm("s0");

    task = arg0;
    switch (task->state) {
        case 0:
            gDisplayState.at100.flags.imageSource = 0;
            Title_SkipFadeFlag                    = 1;
            if ((gDisplayState.field_112 < 0) || (Wip_SysFlags.field_4 != 0)) {
                next               = 6;
                Title_SkipFadeFlag = 0;
            } else {
                Display_SpawnWithOt(Title_TaskDescs, 1, 0, 0);
                gDisplayState.at100.flags.flipMode = 1;
                next                               = task->state + 1;
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
    s8              param2[4];
    u8*             param1;
    register void** scratch asm("s0");
    register u8*    head asm("v1");
    GameSession*    gs;
    u8*             p2;

    scratch = SCRATCH_HEAD_ADDR;
    gs      = gGameSession;
    arg0    = arg0 + 0xA;
    p2      = (u8*)param2;

    head                         = SCRATCH_HEAD_AT(scratch, u8);
    param1                       = head - 8;
    SCRATCH_HEAD_AT(scratch, u8) = param1;

    gs->field_80 = 0;
    param1[3]    = 0;
    param1[2]    = 0x50;
    head[-8]     = 0;

    param2[0] = arg0;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, p2);

    SCRATCH_POP_BYTES_AT(scratch, 8);
}
