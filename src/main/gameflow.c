#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/pad.h"
#include "main/title.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "main/mc.h"

void GameFlow_StateByField34(Task* arg0)
{
    CdCmdQueue*   p;
    register s32  saved asm("s1");
    s8            one;
    u8*           ptr;
    u32           i;
    DisplayState* ds;

    p = &CdCmd_Queue;
    if (arg0->spawnArg1 == 2) {
        if (arg0->state == 0) {
            Pad_SetCooldown(0);
            if (Display_State.field_12c == 0) {
                Display_State.field_12c = 1;
            }
            if (Display_State.field_12c < 0x10) {
                Title_EnqueueDemoScene(Display_State.field_12c - 1);
            }
            arg0->state = arg0->state + 1;
        }
        if (CdCmd_IsIdle() != 0) {
            if (Display_State.field_10e == 0) {
                Display_State.field_10e = 1;
            }
            Title_RestoreDemoCard();
            {
                u8* clearPtr;
                u32 clearI;

                clearPtr = (u8*)Game_Session;
                for (clearI = 0; clearI < sizeof(GameSession); clearI++) {
                    *clearPtr++ = 0;
                }
            }
            ds                     = &Display_State;
            ds->field_101          = 0;
            ds->field_12e          = 0;
            one                    = 1;
            Game_Session->field_4C = one;
            Game_Session->field_80 = 0;
            Snd_SetMutedVolumes(1);
            ds->field_101 = 0;
            ds->field_10b = one;
            Task_Kill(arg0);
            Task_ResetDefaultList();
            Tmd_InitLists();
            Mem_Init();
            Task_Spawn(0, 9, 0, 0);
        }
    } else {
        Display_State.field_12c = 0;
        Pad_SetCooldown(0);
        if (arg0->spawnArg1 == 0) {
            saved = Mc_SaveData.field_21;
            ptr   = (u8*)Game_Session;
            for (i = 0; i < sizeof(GameSession); i++) {
                *ptr++ = 0;
            }
            Display_State.field_101 = 0;
            Display_State.field_12e = 1;
            p->field_248            = 1;
            p->field_244            = 1;
            Wip_SysFlags.field_4    = 1;
            Mc_InitBufferSlots();
            do {
                Mc_SaveData.field_21 = saved;
            } while (0);
            arg0->state = arg0->state + 1;
        } else {
            {
                u8* clearPtr;
                u32 clearI;

                clearPtr = (u8*)Game_Session;
                for (clearI = 0; clearI < sizeof(GameSession); clearI++) {
                    *clearPtr++ = 0;
                }
            }
            Display_State.field_12e = 1;
            Display_State.field_101 = 0;
            p->field_248            = 1;
            p->field_244            = 1;
            Wip_SysFlags.field_4    = 1;
            Game_Session->field_4C  = 1;
        }
        Display_State.field_10b = 1;
        Task_Kill(arg0);
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

    p              = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->x0 = -0xA0;
    p->r0 = r;
    p->g0 = g;
    p->b0 = b;
    yoff  = Display_State.vramYOffset;
    p->w  = 0x140;
    p->h  = 0xF0;
    p->y0 = -0x78 - yoff;
    addPrim(Gpu_CurrentOt - 0x10, p);

    dr             = (DR_TPAGE*)Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 0, 1, (mode & 3) << 5);
    addPrim(Gpu_CurrentOt - 0x10, dr);
}

void Game_ClearSession(void)
{
    u8* ptr;
    u32 i;

    ptr = (u8*)Game_Session;
    for (i = 0; i < sizeof(GameSession); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
}

void GameFlow_InitSystems(void)
{
    Task_ResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void Game_ResetSessionAndBuffers(Task* arg0)
{
    u8*         ptr;
    u32         i;
    s32         saved;
    CdCmdQueue* p;

    p     = &CdCmd_Queue;
    saved = Mc_SaveData.field_21;
    ptr   = (u8*)Game_Session;
    for (i = 0; i < sizeof(GameSession); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
    Display_State.field_12e = 1;
    p->field_248            = 1;
    p->field_244            = 1;
    Wip_SysFlags.field_4    = 1;
    Mc_InitBufferSlots();
    do {
        Mc_SaveData.field_21 = saved;
    } while (0);
    arg0->state = arg0->state + 1;
}

void GameFlow_SpawnMenu(Task* arg0)
{
    void* temp_v0;

    GameMain_SetFrameTiming(0);
    temp_v0         = Ui_SpawnFromDesc(D_800611C8, 0, 1, 0, 0);
    arg0->spawnArg2 = temp_v0;
    if (temp_v0 != 0) {
        Display_State.field_11e = 0xFF;
        Game_Session->field_2   = 1;
        arg0->killCountdown     = 0x10;
        arg0->state             = arg0->state + 1;
    }
}

void GameFlow_WaitMenuDone(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (obj->field_2E == -1) {
        Ui_TeardownTree(obj, obj->owner);
        Display_State.field_11e = 0;
        Game_Session->field_2   = 0;
        if (Mc_SaveData.field_1a9 == 1) {
            CdVol_SetMixMode(0);
        } else {
            CdVol_SetMixMode(1);
        }
        Snd_ApplyVolumeTable(0);
        arg0->killCountdown = 0xC;
        arg0->state         = arg0->state + 1;
    }
}

void GameFlow_CountdownAdvance(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown != 0) {
        return;
    }
    Pad_SetCooldown(0);
    arg0->state = arg0->state + 1;
}

void GameFlow_SpawnMainWhenReady(Task* arg0)
{
    if (Display_State.field_101 == 0) {
        Task_Spawn(0, 2, 0, 0);
        Display_SetMode(0x5010);
        Task_Kill(arg0);
        return;
    }
    Display_State.field_10b = 1;
    Task_Kill(arg0);
    Task_ResetDefaultList();
    Tmd_InitLists();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void GameFlow_DispatchTable5(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = GameFlow_States5;
    sp.funcs[arg0->state](arg0);
}

void GameFlow_CopySaveIds(Task* arg0)
{
    ((SessionBytesAt4*)Game_Session)->field_4 =
        ((SessionBytesAt4*)&Mc_SaveData)->field_4;
    D_8007A394  = 0;
    arg0->state = arg0->state + 1;
}

void GameFlow_EnqueueDefaultLoad(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)func_80042500() == 0) {
        Fs_BeginBootLoad(&Game_Session->field_4, 0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        arg0->state = arg0->state + 1;
    }
}

void Game_ClearEd68(void)
{
    D_8005ED68 = 0;
}

void GameFlow_SpawnWhenIdle(Task* arg0)
{
    if (CdCmd_IsIdle() != 0) {
        Task_Spawn(0, 0x11, 1, 0);
        Task_Kill(arg0);
    }
}

void GameFlow_DispatchTable(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = GameFlow_States3;
    Pad_SetCooldown(0);
    sp.funcs[arg0->state](arg0);
}

void Pad_TickEventBanks(PadState* arg0)
{
    PadState*    pad;
    register u8* temp asm("t0");
    s32          i;
    register u8* p0 asm("a1");
    register u8* p1 asm("a2");
    s32          half;
    u8           val;
    u8           one;

    pad = arg0;
    {
        register void** scratch asm("a0");
        register void*  head asm("v1");
        register void*  alloc asm("v0");

        scratch         = (void**)G_SCRATCH_HEAD;
        p0              = &pad->events[0][0].field_0;
        i               = 0;
        one             = 1;
        head            = *scratch;
        p1              = &pad->events[0][0].field_1;
        alloc           = (u8*)head - 4;
        temp            = alloc;
        *scratch        = alloc;
        temp[1]         = 0;
        ((u8*)head)[-4] = 0;
    }

    do {
        if (*p0 != 0) {
            half                     = *(volatile u16*)(p1 + 1) - 1;
            *(volatile u16*)(p1 + 1) = half;
            if ((half << 16) == 0) {
                *p0 = 0;
            }
            if (*p1 != 0) {
                *temp = one;
            }
        }
        i  += 1;
        p1 += 4;
        p0 += 4;
    } while (i < 8);

    {
        register u8* p1b asm("a0");

        p0  = &pad->events[1][0].field_0;
        i   = 0;
        p1b = &pad->events[1][0].field_1;
        do {
            if (*p0 != 0) {
                half                      = *(volatile u16*)(p1b + 1) - 1;
                *(volatile u16*)(p1b + 1) = half;
                if ((half << 16) == 0) {
                    *p0 = 0;
                }
                val = *p1b;
                if (temp[1] < val) {
                    temp[1] = val;
                }
            }
            i   += 1;
            p1b += 4;
            p0  += 4;
        } while (i < 8);
    }

    if (Mc_SaveData.field_21 == 0) {
        pad->field_5A = temp[0];
        pad->field_5B = temp[1];
    } else {
        pad->field_5A = 0;
        pad->field_5B = 0;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 4;
}

INCLUDE_ASM("main/nonmatchings/gameflow", func_8002C1D8);

void Pad_UpdatePort0(void)
{
    s32                  i;
    s32                  offset;
    PadRawPort*          raw;
    DisplayState*        ds;
    register PadScratch* scratch asm("s1");
    PadState*            pad;
    u16                  buttons;
    u16                  prev;
    register void**      head asm("v1");
    register void*       tmp asm("v0");

    head    = (void**)G_SCRATCH_HEAD;
    i       = 0;
    ds      = &Display_State;
    raw     = Pad_RawPorts;
    offset  = i;
    tmp     = *head;
    tmp     = (u8*)tmp - 6;
    scratch = tmp;
    *head   = scratch;

    do {
        pad = (PadState*)((u8*)Pad_States + offset);
        if (pad->cooldown == 0) {
            scratch->rawHi = raw->field_2;
            scratch->rawLo = raw->field_3;
            TOUCH_REG(raw);
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
                    if (ds->field_1e == 0) {
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

            if (*(s8*)&Game_Session->field_2 != 0) {
                if ((scratch->prevButtons & 0xF000) == (scratch->buttons & 0xF000)) {
                    pad->autoRepeat = pad->autoRepeat + ds->field_10a;
                } else {
                    pad->autoRepeat = 0;
                }
                if (pad->autoRepeat >= 0x1E) {
                    pad->autoRepeat  = 0x16;
                    pad->prevButtons = pad->prevButtons | (pad->buttons & 0xF000);
                }
            }
        } else {
            {
                volatile u8* cooldown;

                cooldown         = &pad->cooldown;
                *cooldown        = *cooldown - 1;
                pad->prevButtons = 0;
                pad->triggered   = 0;
                pad->buttons     = 0;
                if (*cooldown == 0) {
                    scratch->rawHi = raw->field_2;
                    scratch->rawLo = raw->field_3;
                    TOUCH_REG(raw);
                    buttons          = ~*(u16*)&scratch->rawLo;
                    scratch->buttons = buttons;
                    pad->buttons     = buttons;
                }
            }
        }
        raw++;
        i++;
        offset += 0x5C;
    } while (i <= 0);

    head  = (void**)G_SCRATCH_HEAD;
    *head = (u8*)*head + 6;
}
