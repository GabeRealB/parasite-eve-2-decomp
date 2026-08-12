#include "common.h"
#include "main/gamemain.h"
#include "main/display.h"

#include <psyq/libetc.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "main/boot.h"
#include "main/fs.h"
#include "main/game.h"
#include "main/gpuext.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/unknown_syms.h"

void GameMain_Init(void)
{
    s32 flag; // The indirection is required.

    SetDispMask(0);
    Boot_WaitCdAudioReady();
    ResetCallback();
    Boot_ResetCd(1);
    VSync(10);

    GameResetScratchHead();
    D_8005EC64++;
    Mem_ConfigureAuxHeap(0, 0);
    Mem_Init();
    Task_ResetDefaultList();
    Tmd_InitLists();
    Gfx_InitGraph();

    Mem_Set(&Display_State, 0, sizeof(Display_State));
    Display_State.field_120 = 1;
    Display_State.region    = 0;
    Display_State.field_101 = 0;
    Display_State.field_1d  = -1;
    Display_State.field_1e  = 0;
    Display_State.field_10d = 0;
    Display_State.field_0   = 0;
    Display_State.field_4   = 0;
    Display_State.field_8   = 0;
    Display_State.field_c   = 0;
    Display_State.field_10  = 0;
    Display_State.field_14  = 0;
    GameMain_SetFrameTiming(0);

    D_8005EC70 = 0;
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Spu_WaitDma();
    Snd_SetMutedVolumes(0);
    Boot_InitCdAudio();
    VSyncCallback(Display_VSyncCallback);

    flag                   = 1;
    Display_State.field_1f = flag;
    Display_SetMode(0x1010);
    Mem_Set(Pad_RemapState, 0, 0x1C);
}

void func_80020058(void);

void Display_FlipDraw(s32 arg0)
{
    s32 mode;
    u8  saved;

    mode = D_80070E38 & 0xF;
    if (mode != 2) {
        PutDrawEnv(&Display_State.drawEnv[arg0]);
        PutDispEnv(&Display_State.dispEnv[arg0]);
        if (mode == 0) {
            if (D_8006EC30 != 0) {
                Display_LoadImageStrips(arg0);
                saved                  = Display_State.field_1f;
                Display_State.field_1f = arg0;
                func_80020058();
                Display_State.field_1f = saved;
            }
            DrawOTag(Gpu_OtBuffers[Display_State.field_114].lastTag);
        } else if (D_8006EC30 == 2) {
            Display_LoadImageStrips(arg0);
        } else if (D_8006EC30 == 3) {
            Gfx_LoadImageSlot(Game_Session->field_7, Game_Session->field_6, arg0);
        }
        if ((s8)D_80070E38 < 0x10) {
            DrawOTag(Gpu_OrderingTables[arg0].tag);
        }
    }
}

void Display_VSyncCallback(void)
{
    s32           temp_s4;
    s32           temp_s0;
    s32           stride; // Forces s0*0x14 before (ds+0x20); required for match.
    DisplayState* ds;
    DRAWENV*      drawBase;
    DISPENV*      dispBase;

    temp_s4 = VSync(1);
    if ((s32)D_8005EC70 >= 0) {
        if (((temp_s4 & 0xFFFF) + D_8005EC78) > (D_8005EC6C >> 1)) {
            ds = &Display_State;
            if (ds->vsyncFlag == 0) {
                temp_s0  = D_8005EC70;
                drawBase = ds->drawEnv;
                PutDrawEnv(&drawBase[temp_s0]);
                stride   = temp_s0 * 0x14;
                dispBase = ds->dispEnv;
                PutDispEnv(&dispBase[temp_s0]);
                if (ds->field_100 != 0) {
                    Display_LoadImageStrips(temp_s0);
                }
                func_80020058();
                if (ds->field_104 == 0) {
                    DrawOTag(Gpu_OtBuffers[temp_s0].lastTag);
                }
                D_8005EC70 = -1;
            } else if (ds->vsyncFlag == 1) {
                Display_FlipDraw(D_8005EC70);
                D_8005EC70 = -1;
            }
        }
    }
    D_80070F64 -= 1;
    if (Display_State.field_1e == 0) {
        Display_State.field_0 += 1;
    }
    Display_State.field_c += 1;
    CdAudio_Tick();
    Audio_IrqFrameWork();
    func_8002C1D8();
    D_8005EC74 = VSync(1) - (temp_s4 & 0xFFFF);
}

// Drawn by GameMain_ShowLoading (must stay in .rodata for this TU).
const u8 GameMain_PauseText[] = "PAUSE!";

void GameMain_ShowLoading(s32 arg0)
{
    TextDrawReq   sp10;
    s32           skip;
    s32           one;
    s32           buf;
    s32           stride;
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    DRAWENV*      drawBase;
    DISPENV*      dispBase;
    s8            yoff;

    if (!(D_8005EC80 & ~1)) {
        skip = 0;
        if (((s16)CdCmd_Queue.field_244 != 0) && !(D_8005EC80 & 8)) {
            skip = 1;
        } else if ((Display_State.vsyncFlag == 1) && ((s8)Display_State.field_103 == 2)) {
            skip = 1;
        } else if (Fs_CdOpStatus != 0xFF) {
            skip = 1;
        } else if (GpuExt_IsDisplayEnabled() == 0) {
            skip = 1;
        }
        if (skip == 0) {
            D_8005EC80 |= 1 << arg0;
            one         = 1;
            tile        = &D_8006EC18;
            dr          = &D_8006EC28;
            if (arg0 == one) {
                SndEvt_EnqueueTypeD();
            }
            setlen(dr, one);
            dr->code[0] = 0xE1000600;
            DrawPrim(dr);

            tile->x0         = -0xA0;
            tile->w          = 0x140;
            tile->h          = 0xF0;
            *(s32*)&tile->r0 = 0;
            setlen(tile, 3);
            ds = &Display_State;
            setcode(tile, 0x62);
            yoff     = ds->vramYOffset;
            tile->y0 = -0x78 - yoff;
            DrawPrim(tile);

            sp10.x          = 0;
            sp10.otIndex    = 4;
            sp10.field_8    = 0x37A78;
            sp10.glyphTable = 4;
            sp10.centerMode = one;
            sp10.field_E    = 0x10;
            sp10.y          = 6 - ds->vramYOffset;
            func_8002E53C(&sp10, GameMain_PauseText);

            buf      = ds->field_1f ^ 1;
            drawBase = ds->drawEnv;
            PutDrawEnv(&drawBase[buf]);
            stride   = buf * 0x14;
            dispBase = ds->dispEnv;
            PutDispEnv(&dispBase[buf]);

            EnterCriticalSection();
            D_8005EC70 = -1;
            ExitCriticalSection();
        }
    }
}

void GameMain_Loop(void)
{
    s32                    s3;
    register DisplayState* ds asm("s2");
    register s32           s4r asm("s4");
    register s32           s5r asm("s5");
    register s32           neg1 asm("s6");
    s32                    ot_hi;
    GpuOtBuf*              otBase;
    s32                    skip;
    s32                    flip;
    s32                    buf;
    s32                    vs;
    s32                    stride;
    CdCmdQueue*            cq;
    CdCmdQueue*            q;
    DRAWENV*               drawBase;
    DISPENV*               dispBase;
    PadState*              ps;
    s32                    t;
    s32                    wt;
    u_long*                ot_local;
    u_long*                p;
    s32                    a0;
    s32                    d710;
    s32                    n;
    u8                     raw;

    s3 = 0;
    {
        register DisplayState* t asm("v0");
        t  = &Display_State;
        ds = &Display_State; /* CSE: lui/addiu v0 + move s2; pure C alias */
        {
            register s32 t4 asm("v0");
            /* depend on t so Display load completes in v0 first */
            __asm__("lui %0, %%hi(D_8005EC80)" : "=r"(t4) : "r"(t));
            __asm__("move %0, %1" : "=r"(s4r) : "r"(t4));
        }
    }
    __asm__("lui %0, %%hi(D_8005EC70)" : "=r"(s5r));
    neg1 = -1;
    __asm__("lui %0, %%hi(Gpu_OtBuffers)" : "=r"(ot_hi));
    {
        register s32 v asm("v0");
        v = *(u8*)&ds->field_1f;
        /* depend on v so addiu cannot hoist above lbu */
        __asm__ volatile("addiu %0, %1, %%lo(Gpu_OtBuffers)"
                         : "=r"(otBase)
                         : "r"(ot_hi), "r"(v));
        __asm__ volatile("sw $zero, %%lo(D_8005EC80)(%0)" ::"r"(s4r) : "memory");
        ds->field_114 = v;
    }
    /* Force s4r/s5r as the only EC80/EC70 bases for the whole function */
    __asm__ volatile("" ::"r"(s4r), "r"(s5r));

    {
        DisplayState* nv = ds;
        for (;;) {
            if (nv->field_11e == 1) {
                goto do_init;
            }
            if (nv->field_11e != 0) {
                goto after_init;
            }
            if (nv->field_130 != 0) {
                goto after_init;
            }
            if (nv->field_12e == 0) {
                goto after_init;
            }
            if (Pad_CheckSpecialCombo() == 0) {
                goto after_init;
            }
        do_init:
            GameMain_Init();
            __asm__ volatile("sw $zero, %%lo(D_8005EC80)(%0)" ::"r"(s4r) : "memory");
        after_init:
            *(u32*)0x1F8003FC = 0x1F8003FC;
            Pad_UpdatePort0();

            ps = (PadState*)Pad_States;
            if (ps->status == 0xFF && ps->cooldown == 0 && nv->field_12e != 0 &&
                nv->field_12f == 0 && nv->field_11e == 0) {
                GameMain_ShowLoading(1);
            } else {
                register s32 _e asm("v0");
                __asm__ volatile("lw %0, %%lo(D_8005EC80)(%1)" : "=r"(_e) : "r"(s4r));
                if (_e & 2) {
                    SndEvt_EnqueueTypeE();
                    __asm__ volatile("lw %0, %%lo(D_8005EC80)(%1)" : "=r"(_e) : "r"(s4r));
                    {
                        register s32 m3 asm("v1");
                        m3  = -3;
                        _e &= m3;
                    }
                    __asm__ volatile("sw %0, %%lo(D_8005EC80)(%1)" ::"r"(_e), "r"(s4r) : "memory");
                }
            }

            ((void (*)(s32))Snd_PollAsync)(0);

            {
                register s32 _e asm("v0");
                __asm__ volatile("lw %0, %%lo(D_8005EC80)(%1)" : "=r"(_e) : "r"(s4r));
                if (_e != 0) {
                    skip = 0;
                    /* && short-circuit: reload EC80 only when field_244 != 0 */
                    if ((s16)CdCmd_Queue.field_244 != 0 &&
                        !(({
                              register s32 _e2 asm("v0");
                              __asm__ volatile("lw %0, %%lo(D_8005EC80)(%1)"
                                               : "=r"(_e2)
                                               : "r"(s4r));
                              _e2;
                          }) &
                          8)) {
                        skip = 1;
                    } else if (nv->vsyncFlag == 1 && (s8)nv->field_103 == 2) {
                        skip = 1;
                    } else if (Fs_CdOpStatus != 0xFF) {
                        skip = 1;
                    } else if (GpuExt_IsDisplayEnabled() == 0) {
                        skip = 1;
                    }
                    if (skip == 0) {
                        register s32 a0 asm("a0");
                        (void)a0;
                        VSync(0);
                        s3 = VSync(1) & 0x7FFF;
                        Boot_DispatchCdCmd();
                        continue;
                    }
                }
            }

            nv->field_14 += 1;
            nv->field_10 += 1;
            if (nv->field_1e != 0) {
                goto do_flip_draw;
            }

            if (nv->field_10d != 0) {
                u8 mode = nv->field_10d;
                if ((s32)(mode << 24) < 0) {
                    goto do_dispatch;
                }
                if (nv->field_1d >= 0) {
                do_dispatch:
                    Display_DispatchModeId(nv->field_10d);
                }
            }
            if (nv->field_1e != 0) {
            do_flip_draw:
                __asm__ volatile("addiu %0, %1, %%lo(Gpu_OtBuffers)" : "=r"(a0) : "r"(ot_hi));
                s3 = Display_FrameFlipDraw(a0, s3, nv->field_114);
                continue;
            }

            flip          = nv->field_114 ^ 1;
            nv->field_10d = 0;
            cq            = &CdCmd_Queue;
            nv->field_114 = flip;
            nv->field_1f  = (u8)nv->field_114;
            nv->field_8  += 1;
            if ((u16)cq->field_222 == 0) {
                t           = nv->field_4 + 1;
                nv->field_4 = t + (D_8005EC68 >> 1);
            }

            ot_local = Gpu_OtTags + (u32)flip * GPU_OT_ENTRIES;
            n        = GPU_OT_ENTRIES;
            /* Keep ot/n live so li a1 lands right after ot setup, before EC68 loads */
            __asm__ volatile("" : "+r"(ot_local), "+r"(n));
            {
                register s32 d710r asm("s0");
                s32          half;
                s32          f10;
                half = D_8005EC68;
                f10  = nv->field_10;
                __asm__ volatile("lui %0, %%hi(Gpu_CurrentOt)" : "=r"(d710r));
                __asm__ volatile("sw %0, %%lo(Gpu_CurrentOt)(%1)" ::"r"(ot_local), "r"(d710r)
                                 : "memory");
                nv->field_10 = f10 + (half >> 1);
                ClearOTagR(ot_local, n);
                d710 = d710r;
            }
            {
                register s32     endp asm("v1");
                register u_long* pr asm("v0");
                __asm__ volatile("lui %0, 0xff" : "=r"(endp));
                __asm__ volatile("lw %0, %%lo(Gpu_CurrentOt)(%1)" : "=r"(pr) : "r"(d710));
                __asm__ volatile("ori %0, %0, 0xffff" : "+r"(endp));
                *pr = (u_long)endp;
                pr += 0x20;
                __asm__ volatile("sw %0, %%lo(Gpu_CurrentOt)(%1)" ::"r"(pr), "r"(d710)
                                 : "memory");
            }

            {
                register s32 idx asm("a0");
                idx        = nv->field_114;
                D_80070EE0 = D_800740E0 + (u32)idx * 0x3000;
                D_80071190 =
                    (DR_TPAGE*)((u8*)D_80068F88 + (u32)idx * ((u32)D_80068F90 >> 1));
                /* Callee reloads Task_DefaultList itself; keep idx in $a0. */
                Task_ExecDefaultList((TaskNode*)idx);
            }

            if (nv->field_1e != 0) {
                continue;
            }

            Boot_DispatchCdCmd();

            {
                register s32 z asm("a0");
                z = 0;
                if ((s16)nv->height == 0x1E0) {
                    __asm__ volatile("sw %0, %%lo(D_8005EC70)(%1)" ::"r"(neg1), "r"(s5r) : "memory");
                    VSync(z);
                    ResetGraph(1);
                    buf      = nv->field_114;
                    drawBase = nv->drawEnv;
                    PutDrawEnv((DRAWENV*)((s32)(buf * 0x5C) + (s32)drawBase));
                    stride   = buf * 0x14;
                    dispBase = nv->dispEnv;
                    {
                        register s32 a0 asm("a0");
                        a0 = (s32)dispBase;
                        PutDispEnv((DISPENV*)(stride + a0));
                    }
                    if (nv->field_100 != 0) {
                        Display_LoadImageStrips(buf);
                    }
                    func_80020058();
                    if (nv->field_104 != 0) {
                        s3 = 0;
                        continue;
                    }
                    DrawOTag(otBase[buf].lastTag);
                    s3 = 0;
                    continue;
                }
            }

            DrawSync(0);
            {
                s32 tmp = VSync(1) - s3;
                q       = cq;
                {
                    register s32  a0r asm("a0");
                    register u32  acc asm("v1");
                    register u32* cursor asm("v0");
                    a0r = tmp & 0x7FFF;
                    if (q->field_240 != 0) {
                        cursor = q->field_19C;
                        acc    = (u32)a0r + q->field_1A0;
                        if (acc < *cursor) {
                            register s32 w asm("v0");
                            do {
                                w = VSync(1);
                                w = (w - s3) & 0x7FFF;
                            } while ((u32)w + q->field_1A0 < *q->field_19C);
                            {
                                s32 new_var = VSync(1);
                                a0r         = (new_var - s3) & 0x7FFF;
                            }
                            acc = q->field_1A0 + (u32)a0r;
                        }
                        q->field_1A0 = acc;
                        {
                            register u32* c asm("v1");
                            register u32* r asm("v0");
                            c = q->field_19C;
                            if (*c != 0) {
                                r            = c + 1;
                                q->field_19C = r;
                                if ((s32)c[1] == neg1) {
                                    r            = c + 2;
                                    q->field_19C = r;
                                }
                            }
                        }
                    }
                    vs = a0r;
                }
            }

            {
                register s32 lim asm("v0");
                lim = D_8005EC6C;
                if (vs < lim) {
                    register s32 vsarg asm("a0");
                    vsarg         = D_8005EC68;
                    nv->vsyncFlag = 0;
                    {
                        s32 _t = nv->field_114;
                        __asm__ volatile("sw %0, %%lo(D_8005EC70)(%1)" ::"r"(_t), "r"(s5r)
                                         : "memory");
                    }
                    VSync(vsarg);
                    {
                        s32 _t;
                        __asm__ volatile("lw %0, %%lo(D_8005EC70)(%1)" : "=r"(_t) : "r"(s5r));
                        if (_t == (u32)neg1) {
                            D_8005EC78 = D_8005EC74;
                            s3         = -D_8005EC74;
                            goto apply_offset;
                        }
                    }
                    a0 = 1;
                } else {
                    register s32 one asm("a0");
                    register s32 m2 asm("v0");
                    one = 1;
                    m2  = -2;
                    __asm__ volatile("" : "+r"(one), "+r"(m2));
                    nv->vsyncFlag = 0;
                    __asm__ volatile("sw %0, %%lo(D_8005EC70)(%1)" ::"r"(m2), "r"(s5r) : "memory");
                    a0 = one;
                }
            }

            D_8005EC78 = 0;
            s3         = VSync(a0) & 0x7FFF;
            buf        = nv->field_114;
            drawBase   = nv->drawEnv;
            PutDrawEnv((DRAWENV*)((s32)(buf * 0x5C) + (s32)drawBase));
            stride   = buf * 0x14;
            dispBase = nv->dispEnv;
            {
                register s32 a0 asm("a0");
                a0 = (s32)dispBase;
                PutDispEnv((DISPENV*)(stride + a0));
            }
            if (nv->field_100 != 0) {
                Display_LoadImageStrips(buf);
            }
            func_80020058();
            if (nv->field_104 == 0) {
                DrawOTag(otBase[buf].lastTag);
            }
            __asm__ volatile("sw %0, %%lo(D_8005EC70)(%1)" ::"r"(neg1), "r"(s5r) : "memory");

        apply_offset:
            raw = *(volatile u8*)&nv->field_126;
            {
                register s32 v1r asm("v1");
                register s32 a0r asm("a0");
                v1r                   = raw << 24;
                a0r                   = v1r >> 24;
                v1r                   = a0r;
                a0r                  += 0x78;
                v1r                  += 0x188;
                nv->vramYOffset       = raw;
                nv->drawEnv[0].ofs[1] = a0r;
                nv->drawEnv[1].ofs[1] = v1r;
            }

            (void)ot_hi;
            (void)otBase;
            (void)s4r;
            (void)s5r;
        }
    }
}

void Gfx_InitCoordinateTrees(void)
{
    MATRIX*        m;
    GsCOORDINATE2* c1;
    GsCOORDINATE2* c2;
    GsCOORDINATE2* c3;
    s32            one;

    *(s32*)&D_80070E94 = ONE;
    one                = ONE;
    m                  = &D_80070E94;
    c1                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c1->sub            = NULL;
    c1->coord.t[0]     = 0;
    c1->coord.t[1]     = 0;
    c1->coord.t[2]     = 0x8000;
    c1->flg            = 0;

    *(s32*)&D_80070E44 = one;
    m                  = &D_80070E44;
    c2                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c2->sub            = c1;
    c2->coord.t[0]     = 0;
    c2->coord.t[1]     = 0;
    c2->coord.t[2]     = 0;
    c2->flg            = 0;

    *(s32*)&D_80070F14 = one;
    m                  = &D_80070F14;
    c3                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c3->sub            = c2;
    c3->coord.t[0]     = 0;
    c3->coord.t[1]     = 0;
    c3->coord.t[2]     = 0;
    c3->flg            = 0;

    gte_SetGeomScreen(0x400);

    *(s32*)&GsWSMATRIX = one;
    m                  = &GsWSMATRIX;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
}

void Display_LoadImageStrips(s32 arg0)
{
    RECT rect;
    s32  var_s0;
    s32  var_s1;
    s32  temp_v1;
    s32  field;
    s8   yoff;

    if (CdCmd_Queue.field_21C == 0) {
        if (Display_State.vramYOffset >= 0) {
            if (arg0 != 0) {
                rect.y = Display_State.vramYOffset + 0x110;
            } else {
                rect.y = Display_State.vramYOffset;
            }
            rect.x = 0;
            rect.w = 0x140;
            rect.h = 0xF0 - Display_State.vramYOffset;
            LoadImage(&rect, (u_long*)Fs_ImgBuffers);
            return;
        }
        if (arg0 == 0) {
            rect.y = 0;
        } else {
            rect.y = 0x110;
        }
        rect.x = 0;
        rect.w = 0x140;
        yoff   = Display_State.vramYOffset;
        rect.h = yoff + 0xF0;
        LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers + ((-yoff) * 0x280)));
        return;
    }
    var_s1 = 0;
    if (CdCmd_Queue.field_21C == 1) {
        arg0  *= 0x110;
        rect.w = 0x10;
        field  = Display_State.vramYOffset;
        rect.y = arg0;
        rect.h = 0xF0;
        if (field > 0) {
            rect.h -= field;
            rect.y  = arg0 + field;
        } else if (field < 0) {
            var_s1 = (-field) * 0x20;
            rect.h = field + 0xF0;
        }
        var_s0 = 0;
        do {
            temp_v1 = var_s0 & 0xFFFF;
            rect.x  = temp_v1 * 0x10;
            LoadImage(&rect, (u_long*)((u8*)Fs_ImgBuffers + (temp_v1 * 0x1E00) + var_s1));
            var_s0++;
        } while ((u32)(var_s0 & 0xFFFF) < 0x14U);
    }
}

void GameMain_SetFrameTiming(s32 arg0)
{
    if (arg0 == 0) {
        Display_State.field_10a = 1;
        D_8005EC68              = 0;
        D_8005EC6C              = 0x106;
    } else if (arg0 == 1) {
        Display_State.field_10a = 2;
        D_8005EC68              = 2;
        D_8005EC6C              = 0x20D;
    } else if (arg0 == 2) {
        Display_State.field_10a = 3;
        D_8005EC68              = 3;
        D_8005EC6C              = 0x313;
    }
}

void Gpu_ClearOTag(s16 tableIdx)
{
    u_long* tableStart = Gpu_OtTags + tableIdx * GPU_OT_ENTRIES;
    ClearOTagR(tableStart, GPU_OT_ENTRIES);
    *tableStart = GPU_OT_END_PRIM;
}

void Gfx_InitGraph(void)
{
    RECT      rect;
    GpuOtBuf* otCtx;
    u_long*   ot;
    s32       depth;

    if (D_8005EC64 == 1) {
        ResetGraph(0);
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x200;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    InitGeom();

    otCtx            = Gpu_OtBuffers;
    depth            = 0xA;
    otCtx->depth     = depth;
    ot               = Gpu_OtTags;
    otCtx->lastTag   = ot + GPU_OT_ENTRIES - 1;
    otCtx->ot        = ot;
    otCtx[1].depth   = depth;
    otCtx[1].ot      = ot + GPU_OT_ENTRIES;
    otCtx[1].lastTag = ot + 2 * GPU_OT_ENTRIES - 1;
    GameMain_SpawnBootTask();
    Gfx_InitCoordinateTrees();
    Gpu_InitDefaultLights();
    Display_State.field_100 = 0;
}

void GameMain_SpawnBootTask(void)
{
    if (D_8005EC64 == 1) {
        Task_Spawn(0, 0x1F, 0, 0);
    } else {
        Task_Spawn(0, 0x20, 0, 0);
    }
}

void Display_PutEnvAndDraw(s32 arg0)
{
    PutDrawEnv(&Display_State.drawEnv[arg0]);
    PutDispEnv(&Display_State.dispEnv[arg0]);
    if (Display_State.field_100 != 0) {
        Display_LoadImageStrips(arg0);
    }
    func_80020058();
    if (Display_State.field_104 == 0) {
        DrawOTag(Gpu_OtBuffers[arg0].lastTag);
    }
}

// TODO
void GameMain(void)
{
    GameResetScratchHead();
    ResetCallback();
    SetVideoMode(MODE_NTSC);
    Spu_Init();
    Mc_InitLib();
    Pad_Init();
    Boot_InitCd();
    Mem_Set(&Wip_SysFlags, 0, sizeof(Wip_SysFlags));
    D_8005EC64 = 0;
    GameMain_Init();
    GameMain_Loop();
}

u32 GameMain_GetResetCount(void)
{
    return D_8005EC64;
}
