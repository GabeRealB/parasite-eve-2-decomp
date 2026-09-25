#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>
#include "actors/actor_136300.h"
#include "actors/actors_shared_80149ed0.h"
#include "main/display.h"
#include "main/task.h"

extern s16 D_800691CA;

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 D_actor_136300_80132ADC;

/// The ramp context the running wave task was spawned with, parked at spawn
/// so the tick reads the ramp through it.
extern Actor136300WaveCtx* D_actor_136300_8013C888;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ActorWaveRec6 D_actor_136300_8013C88C[11];
extern ActorWaveRec6 D_actor_136300_8013C8DC[30];

/// Screen-wave task, spawned by the message handler with the ramp context
/// `D_actor_136300_8013C99C` as its argument. State 0 seeds the column and row
/// phases, parks the argument and clears its ramp; state 1 ramps the frame up
/// to the span (ramp state 0) or back down to zero (ramp state 1, then 2,
/// which kills the task and restores the display field), and redraws the
/// frame buffer as a 10 by 30 mesh of textured quads displaced by sine waves
/// of that amplitude, tinted when the context's tint flag is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `D_800691CA` store, which a member read lets GCC hoist above it.
void func_actor_136300_80131E40(Task* arg0)
{
    Actor136300WaveCtx* ctx;
    POLY_FT4*           p;
    DR_STP*             stp;
    s32                 i, j, k;
    s32                 drawY;
    s32                 tpage0, tpage1;
    s32                 u0, u1, v0, v1;
    s32                 waveX0, waveY0, waveX1, waveY1;
    s32                 waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_136300_8013C88C[i].phase  = 0;
                D_actor_136300_8013C88C[i].offset = (u32)rand() >> 3;
                D_actor_136300_8013C88C[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_136300_8013C8DC[i].phase  = 0;
                D_actor_136300_8013C8DC[i].offset = (u32)rand() >> 3;
                D_actor_136300_8013C8DC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_136300_80132ADC          = 0;
            D_actor_136300_8013C888          = arg0->spawnArg2;
            D_actor_136300_8013C888->field_6 = 0;
            D_actor_136300_8013C888->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_actor_136300_8013C888;
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
            D_actor_136300_80132ADC = D_actor_136300_8013C888->field_6 * D_actor_136300_8013C888->field_2 / D_actor_136300_8013C888->field_0;
            for (i = 0; i < 11; i++) {
                D_actor_136300_8013C88C[i].phase += D_actor_136300_8013C88C[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_136300_8013C8DC[i].phase += D_actor_136300_8013C8DC[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_136300_8013C888->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_136300_8013C888->field_9;
                        p->g0 = D_actor_136300_8013C888->field_A;
                        p->b0 = D_actor_136300_8013C888->field_B;
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
                        waveX0 = D_actor_136300_80132ADC * (rsin((j << 9) + D_actor_136300_8013C88C[k].phase + D_actor_136300_8013C88C[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_136300_80132ADC * (rsin((k << 10) + D_actor_136300_8013C8DC[j].phase + D_actor_136300_8013C8DC[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_136300_80132ADC * (rsin((j << 9) + D_actor_136300_8013C88C[k + 1].phase + D_actor_136300_8013C88C[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_136300_80132ADC * (rsin(((k + 1) << 10) + D_actor_136300_8013C8DC[j].phase + D_actor_136300_8013C8DC[j].offset) << 3);
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

                        waveX2 = D_actor_136300_80132ADC * (rsin(((j + 1) << 9) + D_actor_136300_8013C88C[k].phase + D_actor_136300_8013C88C[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_136300_80132ADC * (rsin((k << 10) + D_actor_136300_8013C8DC[j + 1].phase + D_actor_136300_8013C8DC[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_136300_80132ADC * (rsin(((j + 1) << 9) + D_actor_136300_8013C88C[k + 1].phase + D_actor_136300_8013C88C[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_136300_80132ADC * (rsin(((k + 1) << 10) + D_actor_136300_8013C8DC[j + 1].phase + D_actor_136300_8013C8DC[j + 1].offset) << 3);
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
