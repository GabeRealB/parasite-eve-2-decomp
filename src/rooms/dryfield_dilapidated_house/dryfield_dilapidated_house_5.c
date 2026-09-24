#include "common.h"

#include <psyq/abs.h>

#include "actors/actors_shared_80149ed0.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"

extern s16 D_800691CA;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_dryfield_dilapidated_house_80183E60;

/// The ramp and tint the wave task was spawned with.
extern ActorWaveCtx* D_dryfield_dilapidated_house_80189B74;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern ActorWaveRec6 D_dryfield_dilapidated_house_80189B84[11];
extern ActorWaveRec6 D_dryfield_dilapidated_house_80189BD4[30];

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `Display_ClampField126`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front. The state is read through a plain word
/// load at its offset, which keeps it ordered after the store to `D_800691CA`.
void func_dryfield_dilapidated_house_8017D64C(Task* arg0)
{
    ActorWaveCtx* ctx;
    POLY_FT4*     p;
    DR_STP*       stp;
    s32           i, j, k;
    s32           drawY;
    s32           tpage0, tpage1;
    s32           u0, u1, v0, v1;
    s32           waveX0, waveY0, waveX1, waveY1;
    s32           waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + 0x30)) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase  = 0;
                D_dryfield_dilapidated_house_80189B84[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189B84[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase  = 0;
                D_dryfield_dilapidated_house_80189BD4[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189BD4[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_dryfield_dilapidated_house_80183E60          = 0;
            D_dryfield_dilapidated_house_80189B74          = arg0->spawnArg2;
            D_dryfield_dilapidated_house_80189B74->field_6 = 0;
            D_dryfield_dilapidated_house_80189B74->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_dryfield_dilapidated_house_80189B74;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        ctx->field_6--;
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_dryfield_dilapidated_house_80183E60 = D_dryfield_dilapidated_house_80189B74->field_6 * D_dryfield_dilapidated_house_80189B74->field_2 / D_dryfield_dilapidated_house_80189B74->field_0;
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase += D_dryfield_dilapidated_house_80189B84[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase += D_dryfield_dilapidated_house_80189BD4[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_dryfield_dilapidated_house_80189B74->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_dryfield_dilapidated_house_80189B74->field_9;
                        p->g0 = D_dryfield_dilapidated_house_80189B74->field_A;
                        p->b0 = D_dryfield_dilapidated_house_80189B74->field_B;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}
