#include "common.h"
#include <psyq/abs.h>
#include "main/task.h"
#include "main/display.h"
#include "main/mem.h"
#include "actors/actors_shared_80149ed0.h"
#include "actors/actor_206100.h"

extern s16 D_800691CA;

/// Distortion amplitude of the screen wave: `frame * scale / span` of the
/// running spawn argument, recomputed every frame.
extern s32 D_actor_206100_80158B08;

/// The spawn argument of the running wave task, parked at spawn so the tick
/// reads the ramp through it.
extern Actor206100FlashArg* D_actor_206100_80158BA8;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ActorWaveRec6 D_actor_206100_80158BAC[11];
extern ActorWaveRec6 D_actor_206100_80158BFC[30];

/// Screen-wave child `func_actor_206100_8014CB68` starts off
/// `D_actor_206100_80158AF0`.  State 0 seeds the column and row phases, parks
/// the spawn argument and clears its ramp; state 1 ramps `frame` up to `span`
/// (ramp state 0) or back down to zero (ramp state 1, then 2, which kills the
/// task and restores the display field), and redraws the frame buffer as a
/// 10 by 30 mesh of textured quads displaced by sine waves of that amplitude,
/// modulated by the tint when `blend` is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `D_800691CA` store, which a member read lets GCC hoist above it.
void func_actor_206100_80149ED0(Task* task)
{
    Actor206100FlashArg* ctx;
    POLY_FT4*            p;
    DR_STP*              stp;
    s32                  i, j, k;
    s32                  drawY;
    s32                  tpage0, tpage1;
    s32                  u0, u1, v0, v1;
    s32                  waveX0, waveY0, waveX1, waveY1;
    s32                  waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)task + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_206100_80158BAC[i].phase  = 0;
                D_actor_206100_80158BAC[i].offset = (u32)rand() >> 3;
                D_actor_206100_80158BAC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_206100_80158BFC[i].phase  = 0;
                D_actor_206100_80158BFC[i].offset = (u32)rand() >> 3;
                D_actor_206100_80158BFC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_206100_80158B08        = 0;
            D_actor_206100_80158BA8        = task->spawnArg2;
            D_actor_206100_80158BA8->frame = 0;
            D_actor_206100_80158BA8->state = 0;
            Display_ClampField126(-8);
            task->state++;
            break;
        case 1:
            ctx = D_actor_206100_80158BA8;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(task);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_206100_80158B08 = D_actor_206100_80158BA8->frame * D_actor_206100_80158BA8->scale / D_actor_206100_80158BA8->span;
            for (i = 0; i < 11; i++) {
                D_actor_206100_80158BAC[i].phase += D_actor_206100_80158BAC[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_206100_80158BFC[i].phase += D_actor_206100_80158BFC[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_206100_80158BA8->blend == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_206100_80158BA8->r;
                        p->g0 = D_actor_206100_80158BA8->g;
                        p->b0 = D_actor_206100_80158BA8->b;
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
                        waveX0 = D_actor_206100_80158B08 * (rsin((j << 9) + D_actor_206100_80158BAC[k].phase + D_actor_206100_80158BAC[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_206100_80158B08 * (rsin((k << 10) + D_actor_206100_80158BFC[j].phase + D_actor_206100_80158BFC[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_206100_80158B08 * (rsin((j << 9) + D_actor_206100_80158BAC[k + 1].phase + D_actor_206100_80158BAC[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_206100_80158B08 * (rsin(((k + 1) << 10) + D_actor_206100_80158BFC[j].phase + D_actor_206100_80158BFC[j].offset) << 3);
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

                        waveX2 = D_actor_206100_80158B08 * (rsin(((j + 1) << 9) + D_actor_206100_80158BAC[k].phase + D_actor_206100_80158BAC[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_206100_80158B08 * (rsin((k << 10) + D_actor_206100_80158BFC[j + 1].phase + D_actor_206100_80158BFC[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_206100_80158B08 * (rsin(((j + 1) << 9) + D_actor_206100_80158BAC[k + 1].phase + D_actor_206100_80158BAC[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_206100_80158B08 * (rsin(((k + 1) << 10) + D_actor_206100_80158BFC[j + 1].phase + D_actor_206100_80158BFC[j + 1].offset) << 3);
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
