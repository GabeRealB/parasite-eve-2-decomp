#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "main/mem.h"
#include "actors/actors_shared_80149e54.h"
#include <psyq/abs.h>
#include "actors/actors_shared_80149ed0.h"
extern s16           D_800691CA;
extern s32           ActorsShared80149ed0Amplitude;
extern ActorWaveCtx* ActorsShared80149ed0Context;
extern ActorWaveRec6 ActorsShared80149ed0Columns[11];
extern ActorWaveRec6 ActorsShared80149ed0Rows[30];

void ActorsShared80149ed0(Task* arg0)
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
                ActorsShared80149ed0Columns[i].phase  = 0;
                ActorsShared80149ed0Columns[i].offset = (u32)rand() >> 3;
                ActorsShared80149ed0Columns[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                ActorsShared80149ed0Rows[i].phase  = 0;
                ActorsShared80149ed0Rows[i].offset = (u32)rand() >> 3;
                ActorsShared80149ed0Rows[i].speed  = (rand() * 100 + 20) >> 15;
            }
            ActorsShared80149ed0Amplitude        = 0;
            ActorsShared80149ed0Context          = arg0->spawnArg2;
            ActorsShared80149ed0Context->field_6 = 0;
            ActorsShared80149ed0Context->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = ActorsShared80149ed0Context;
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
            ActorsShared80149ed0Amplitude = ActorsShared80149ed0Context->field_6 * ActorsShared80149ed0Context->field_2 / ActorsShared80149ed0Context->field_0;
            for (i = 0; i < 11; i++) {
                ActorsShared80149ed0Columns[i].phase += ActorsShared80149ed0Columns[i].speed;
            }
            for (i = 0; i < 30; i++) {
                ActorsShared80149ed0Rows[i].phase += ActorsShared80149ed0Rows[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (ActorsShared80149ed0Context->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = ActorsShared80149ed0Context->field_9;
                        p->g0 = ActorsShared80149ed0Context->field_A;
                        p->b0 = ActorsShared80149ed0Context->field_B;
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
                        waveX0 = ActorsShared80149ed0Amplitude * (rsin((j << 9) + ActorsShared80149ed0Columns[k].phase + ActorsShared80149ed0Columns[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = ActorsShared80149ed0Amplitude * (rsin((k << 10) + ActorsShared80149ed0Rows[j].phase + ActorsShared80149ed0Rows[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = ActorsShared80149ed0Amplitude * (rsin((j << 9) + ActorsShared80149ed0Columns[k + 1].phase + ActorsShared80149ed0Columns[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = ActorsShared80149ed0Amplitude * (rsin(((k + 1) << 10) + ActorsShared80149ed0Rows[j].phase + ActorsShared80149ed0Rows[j].offset) << 3);
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

                        waveX2 = ActorsShared80149ed0Amplitude * (rsin(((j + 1) << 9) + ActorsShared80149ed0Columns[k].phase + ActorsShared80149ed0Columns[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = ActorsShared80149ed0Amplitude * (rsin((k << 10) + ActorsShared80149ed0Rows[j + 1].phase + ActorsShared80149ed0Rows[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = ActorsShared80149ed0Amplitude * (rsin(((j + 1) << 9) + ActorsShared80149ed0Columns[k + 1].phase + ActorsShared80149ed0Columns[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = ActorsShared80149ed0Amplitude * (rsin(((k + 1) << 10) + ActorsShared80149ed0Rows[j + 1].phase + ActorsShared80149ed0Rows[j + 1].offset) << 3);
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
