#include "common.h"

#include "main/game.h"
#include "main/title.h"

#include <psyq/libgpu.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

// Absolute main-BSS aliases used for title-overlay match (same addrs as
// Display_State / Wip_SysFlags fields or neighboring flags).
extern u8           D_80071068; // Display_State.field_100
extern s8           D_800710A9; // Wip_SysFlags.unknown_0[1]
extern s16          D_800710AC; // Wip_SysFlags.field_4
extern u16          D_80071094; // Display_State.field_12c
extern u8           D_80071086; // Display_State.field_11e
extern u_long*      Gpu_CurrentOt;
extern DR_TPAGE*    D_80071190;
extern WipUiHolder* Wip_UiHolder;
extern u16*         D_8005C374;
extern u8           D_800733F0[2][0x6C];
extern u8           D_800734C8[2][0xB0];
extern u8           D_80073628[2][0x24];
extern u8           D_80073670[2][0xE4];
extern u8           D_80073838[2][0xA4];
extern u8           D_80073980[0x208];
extern s32          D_8005ED70;
/// Alias of CdCmd_Queue.field_23E (absolute form matches title overlay).
extern s16 D_800691DE;

void func_807246B4(void);

/* Package header: title_rodata.c. This TU’s .rodata is only switch jtables. */

void Title_InitTask(Task* arg0)
{
    register s32  flag asm("s2");
    DisplayState* ds;
    TitleWork*    work;

    flag          = 1;
    ds            = &Display_State;
    ds->field_100 = 0;
    Wip_UiHolder  = NULL;
    if (arg0->spawnArg1 < 0) {
        flag             = 0;
        arg0->spawnArg1 &= 0x7FFFFFFF;
    }
    if (arg0->spawnArg1 > 0) {
        arg0->spawnArg1 -= 1;
        return;
    }
    work = Mem_Calloc(0x18, 0);
    if (work != NULL) {
        arg0->idMap                           = (TaskIdMap*)work;
        *(volatile s32*)&work->menuCount      = 5;
        *(volatile s32*)&work->selection      = 2;
        *(volatile s32*)&work->fadeTileEnable = flag;
        *(volatile s32*)&work->timer          = 0;
        if (D_800710A9 != 0) {
            work->selection = 3;
        }
        Text_LoadClutImages();
        Display_SetMode(0x9010);
        ds->field_1d  = -1;
        work->timer   = -0x10;
        ds->field_100 = 1;
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

    c             = color;
    p             = (SPRT*)D_80071190;
    D_80071190    = (DR_TPAGE*)(p + 1);
    p->x0         = -0x80;
    p->w          = 0x100;
    p->h          = 0x10;
    p->clut       = 0x3FC0;
    *(s32*)&p->r0 = (c << 16) | (c << 8) | c;
    setlen(p, 4);
    p->u0 = 0;
    p->v0 = v;
    setcode(p, 0x66);
    p->y0 = y;
    addPrim(Gpu_CurrentOt, p);

    dr         = D_80071190;
    D_80071190 = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE10002BC;
    addPrim(Gpu_CurrentOt, dr);
}

void Title_MenuTask(Task* arg0)
{
    register Task*      s4 asm("s4");
    register TitleWork* s3 asm("s3");
    s32                 prev;
    s32                 cur;
    s32                 color;
    s32                 fade;
    DisplayState*       ds;

    s4        = arg0;
    s3        = (TitleWork*)s4->idMap;
    prev      = s3->timer;
    cur       = prev + 1;
    s3->timer = cur;

    if (cur < 0x385) {
        goto normal;
    }
    if (cur >= 0x394) {
        goto exit_path;
    }
    if (s3->fadeTileEnable != 0) {
        /* 939C4-like free allocation for constant order; keep prev live as a1 */
        register s32   a1 asm("a1");
        register TILE* a0 asm("a0");
        DR_TPAGE*      dr;
        s32            c;
        s32            h;

        a1 = prev; /* pin after prologue so color uses $a1 */
        h  = 0xF0;

        a0         = (TILE*)D_80071190;
        D_80071190 = (DR_TPAGE*)(a0 + 1);
        setlen(a0, 3);
        setcode(a0, 0x60);
        c      = a1 * 16;
        c      = c - 0x3831;
        a0->b0 = c;
        a0->g0 = c;
        a0->r0 = c;
        asm("" : "+r"(a1));
        a0->x0 = -0xA0;
        a0->y0 = -0x78;
        a0->w  = 0x140;
        a0->h  = h;
        setSemiTrans(a0, 1);
        addPrim(Gpu_CurrentOt, a0);

        dr         = D_80071190;
        D_80071190 = dr + 1;
        setlen(dr, 1);
        /* Split like Title_DrawSpriteRow: lui 0xE100 / ori 0x240, after 0xFFFFFF */
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt, dr);
    }
    goto end;

exit_path:
    Wip_SysFlags.field_4 = 0;
    if (Wip_SysFlags.unknown_0[0] == 1) {
        asm volatile("" ::: "a0");
        Task_CallExit(s4);
        {
            register u32 v0 asm("v0");
            v0 = GameMain_GetResetCount();
            ds = &Display_State;
            asm("" : "+r"(v0), "+r"(ds));
            v0            = v0 + 2;
            ds->field_12c = v0;
            asm("" : "+r"(v0), "+m"(ds->field_12c));
            {
                register u32 a1 asm("a1");
                a1            = v0 & 0xFFFF;
                a1            = a1 % 3;
                a1            = a1 + 1;
                ds->field_12c = a1;
                printf(Title_DemoStartMsg, a1);
            }
            Task_Spawn(0, 3, 2, 0);
            ds->field_100 = 0;
        }
    } else {
        D_80071086 = 1;
    }
    goto end;

normal:
    if (s3->fadeTileEnable != 0) {
        if (cur < 0) {
            TILE*     p;
            DR_TPAGE* dr;
            s32       tmp;

            p          = (TILE*)D_80071190;
            D_80071190 = (DR_TPAGE*)(p + 1);
            setlen(p, 3);
            setcode(p, 0x62);
            tmp   = s3->timer;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;
            color = ~(tmp << 4);
            p->b0 = color;
            p->g0 = color;
            p->r0 = color;
            addPrim(Gpu_CurrentOt, p);

            dr         = D_80071190;
            D_80071190 = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000000 | 0x240;
            addPrim(Gpu_CurrentOt, dr);
        }
    }

    if (s4->state != 3) {
        goto intro;
    }

    if (s3->menuFade < 0x80) {
        s3->menuFade = s3->menuFade + 8;
    }

    {
        register s32 s2 asm("s2");
        register s32 s1 asm("s1");
        register s32 s0 asm("s0");

        s2 = 0;
        s1 = 0x30;
        s0 = 0x38;
        do {
            register s32 a0 asm("a0");
            register s32 a1 asm("a1");
            register s32 a2 asm("a2");
            a0  = s0;
            a1  = s1;
            a2  = s3->menuFade;
            s1 += 0x10;
            s0 += 0xE;
            asm("" : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(s0), "+r"(s1));
            Title_DrawSpriteRow(a0, a1, a2);
            s2 += 1;
        } while (s2 < 3);
    }

    Title_DrawSpriteRow(((s3->selection - 2) * 0xE) + 0x38, 0x20, s3->menuFade);

    {
        s32 f = s3->menuFade;
        s32 q = f;
        if (q < 0) {
            q += 7;
        }
        Title_DrawSpriteRow((q >> 3) + 0x40, 0, 0x80 - f);
    }
    Title_DrawSpriteRow(0x5C, 0x10, 0x80 - s3->menuFade);

    if (s3->menuFade < 0x80) {
        goto end;
    }

    if (Pad_CheckButtons(0, 1, 0x4000) != 0) {
        s3->timer     = 0;
        s3->selection = s3->selection + 1;
        SndEvt_EnqueueType6(2, 0, 0);
        if (s3->selection >= s3->menuCount) {
            s3->selection = s3->selection - s3->menuCount;
        }
        if (s3->selection == 0) {
            s3->selection = 1;
        }
        if (s3->selection == 1) {
            s3->selection = 2;
        }
        goto end;
    }

    if (Pad_CheckButtons(0, 1, 0x1000) != 0) {
        s3->timer     = 0;
        s3->selection = s3->selection - 1;
        SndEvt_EnqueueType6(2, 0, 0);
        if (s3->selection == 1) {
            s3->selection = 0;
        }
        if (s3->selection == 0) {
            s3->selection = -1;
        }
        if (s3->selection < 0) {
            s3->selection = s3->selection + s3->menuCount;
        }
        goto end;
    }

    if (Pad_CheckButtons(0, 1, D_8005ED70 | 0x800) != 0) {
        SndEvt_EnqueueType6(3, 0, 0);
        Task_Spawn(0, Title_MenuSpawnIds[s3->selection], 0, 0);
        D_80071068 = 0;
        Task_CallExit(s4);
    }
    goto end;

intro:
    if (s3->logoFade < 0x80) {
        s3->logoFade = s3->logoFade + 8;
    }
    fade = s3->logoFade;
    {
        s32 q = 0x80 - fade;
        if (q < 0) {
            q += 7;
        }
        Title_DrawSpriteRow(0x40 - (q >> 3), 0, fade);
    }
    Title_DrawSpriteRow(0x5C, 0x10, 0x80);
    if (Pad_CheckButtons(0, 1, D_8005ED70 | 0x800) != 0) {
        SndEvt_EnqueueType6(3, 0, 0);
        s3->timer = 0;
        s4->state = s4->state + 1;
    }
end:
    return;
}

/// Restore demo card / save banks from D_8005C374 (or 0x80600100 when D_80071094 == 0x10).
/// Preserves Mc_SaveData.field_21 / field_23 across the bulk copy.
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
    saveField23 = Mc_SaveData.field_23;
    saveField21 = Mc_SaveData.field_21;
    if (D_80071094 == 0x10) {
        src = (u8*)0x80600100;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData.field_7, Mc_SaveData.field_6);

    memcpy(&Mc_SaveData, src, sizeof(McSaveData));
    src += sizeof(McSaveData);

    memcpy((u8*)&Wip_SysConfig + bank * 0x40, src, 0x40);
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

    Mc_SaveData.field_23 = saveField23;
    Mc_SaveData.field_21 = saveField21;
    if (Fs_StageCdfIsAvailable(Mc_SaveData.field_7) != 1) {
        D_80071086 = 1;
    }
    printf(Title_DemoCardRestoreMsg, Mc_SaveData.field_7, Mc_SaveData.field_6);
}

void Title_FlagAdvanceTask(Task* arg0)
{
    s32* p = &arg0->state;

    D_80071068 = 1;
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
    GBytes8                key;
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
    key = ((SessionBytesAt4*)Game_Session)->field_4;
    if (Wip_SysFlags.unknown_0[0] == 2) {
        key.data[0] = 0x65;
    } else {
        key.data[0] = 0x64;
    }
    slot = Stream_FindSlot(key.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        asm("" : "+r"(cmd), "+r"(zero), "+r"(p), "+r"(slot));
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
        asm("" : "+r"(mask));
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
        asm("" : "+r"(cmd), "+r"(p1), "+r"(p2));
        D_800691DE = 1;
        param1[3]  = 0;
        param1[2]  = 0;
        param1[0]  = 2;
        param2[0]  = 0;
        param2[1]  = 0;
        param2[2]  = 0;
        param2[3]  = 0;
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
    ds = &Display_State;
    Display_LoadImageStrips(ds->field_1f);
    Display_LoadImageStrips(ds->field_1f ^ 1);
    ds->field_100 = 1;
advance:
    task->state = task->state + 1;
    return;

L_case7:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    Task_Kill(task);
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
            Display_State.field_100 = 0;
            Title_SkipFadeFlag      = 1;
            if ((Display_State.field_112 < 0) || (D_800710AC != 0)) {
                next               = 6;
                Title_SkipFadeFlag = 0;
            } else {
                Display_SpawnWithOt(Title_TaskDescs, 1, 0, 0);
                Display_State.field_103 = 1;
                next                    = task->state + 1;
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
            D_800710AC = 1;
            Task_Kill(task);
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
    register void*  head asm("v1");
    GameSession*    gs;
    u8*             p2;

    scratch = (void**)G_SCRATCH_HEAD;
    gs      = Game_Session;
    arg0    = arg0 + 0xA;
    p2      = (u8*)param2;

    head     = *scratch;
    param1   = (u8*)head - 8;
    *scratch = param1;

    gs->field_80    = 0;
    param1[3]       = 0;
    param1[2]       = 0x50;
    ((u8*)head)[-8] = 0;

    param2[0] = arg0;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, p2);

    *scratch = (u8*)*scratch + 8;
}
