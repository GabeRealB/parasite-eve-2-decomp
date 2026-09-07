#include "common.h"

#include "aya/aya_20900.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s8 D_80071068;

void func_aya_20900_8011578C(Task* arg0)
{
    TILE*     p;
    DR_TPAGE* dr;
    u32*      buf;
    u32       i;
    u32       val;
    u8        color;
    char      pad[8];

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
        case 1:
            arg0->state += 1;
            break;
        case 2:
            buf = (u32*)Fs_ImgBuffers;
            val = 0x1F001F;
            i   = 0;
            do {
                *buf++ = val;
                i++;
            } while ((i & 0xFFFF) <= 0x95FFU);
            arg0->state += 1;
            break;
        case 3:
            arg0->killCountdown += 8;
            break;
    }

    if (arg0->killCountdown >= 0x100) {
        Task_Kill(arg0);
        return;
    }

    p              = (TILE*)Gpu_PrimCursor;
    color          = ~(u8)arg0->killCountdown;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(Gpu_CurrentOt - 0x10, p);

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000000 | 0x220;
    addPrim(Gpu_CurrentOt - 0x10, dr);
}

void func_aya_20900_80115948(void)
{
    POLY_FT4* p;
    s16       x;
    s16       y;
    s16       w;
    s16       h;

    x              = -0x3C;
    p              = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 9);
    setcode(p, 0x2D);
    y = -0xC;
    w = 0x78;
    h = 0x18;
    setXYWH(p, x, y, w, h);
    setUVWH(p, 0, 0, w, h);
    p->clut  = 0x3FC0;
    p->tpage = 0x2F;
    addPrim(Gpu_CurrentOt, p);
}

s32 func_aya_20900_80115A14(Task* arg0)
{
    Aya20900Work* work;
    TILE*         p;
    DR_TPAGE*     dr;
    u16           fade;
    u16           timer;
    s32           showLogo;
    u8            color;

    work     = (Aya20900Work*)arg0->idMap;
    showLogo = 1;
    switch ((s16)work->index) {
        case 0:
            if (Game_Session->field_128 == 3) {
                showLogo     = 0;
                work->fade   = 0;
                work->index += 1;
            } else {
                Mem_Set(Fs_ImgBuffers, 0, 0x25800);
                D_80071068  = 1;
                work->fade  = 0xFF;
                work->index = 2;
            }
            break;
        case 1:
            fade       = work->fade + 8;
            work->fade = fade;
            if ((s16)fade >= 0xFF) {
                Mem_Set(Fs_ImgBuffers, 0, 0x25800);
                D_80071068   = 1;
                work->fade   = 0xFF;
                work->index += 1;
            }
            showLogo = 0;
            break;
        case 3:
            SetDispMask(1);
            goto fade_out;
        case 2:
            work->index += 1;
        fade_out:
            fade       = work->fade - 4;
            work->fade = fade;
            if ((s16)fade <= 0) {
                work->fade   = 0;
                work->timer  = 0;
                work->index += 1;
            }
            break;
        case 4:
            timer       = work->timer + 1;
            work->timer = timer;
            if ((s16)timer < 0x30D) {
                if (Pad_CheckFlag800() != 0) {
                    SndEvt_EnqueueType2(0x62, 1);
                    work->index += 1;
                }
            } else {
                work->index += 1;
            }
            break;
        case 5:
            fade       = work->fade + 8;
            work->fade = fade;
            if ((s16)fade >= 0xFF) {
                D_80071068 = 0;
                return 1;
            }
            break;
    }

    if (showLogo & 0xFFFF) {
        func_aya_20900_80115948();
    }

    p              = (TILE*)Gpu_PrimCursor;
    color          = (u8)work->fade;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(Gpu_CurrentOt - 0x10, p);

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000000 | 0x240;
    addPrim(Gpu_CurrentOt - 0x10, dr);
    return 0;
}

void func_aya_20900_80115CFC(Task* arg0)
{
    TaskIdMap* temp_v0;
    s32        temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            temp_v0     = Mem_Malloc(8U, false);
            arg0->idMap = temp_v0;
            if (temp_v0 == NULL) {
                Task_Kill(arg0);
                return;
            }
            Mem_Set(temp_v0, 0U, 8U);
            arg0->state += 1;
        case 1:
            if ((func_aya_20900_80115A14(arg0) << 0x10) != 0) {
                Task_RequestKill(arg0, 0);
            }
            return;
    }
}
