#include "common.h"
#include <psyq/abs.h>

#include "actors/actors_shared_80149e54.h"
#include "gameplay/3A34.h"
#include "main/display.h"
#include "main/mem.h"

extern s16           D_800691CA;
extern u8            D_80070F87;
extern u8            D_801153F4;
extern s32           D_shelter_b3_garbage_incinerator_80185BC4;
extern ActorWaveCtx* D_shelter_b3_garbage_incinerator_8018FC38;
extern ActorWaveRec  D_shelter_b3_garbage_incinerator_8018FC60[9];
extern ActorWaveRec  D_shelter_b3_garbage_incinerator_8018FCB0[30];
extern POLY_FT4      D_shelter_b3_garbage_incinerator_8018FEE0[][30][8];

/// Screen-wave effect task, driven by the context passed as its spawn
/// argument. Its first run seeds a random phase offset and speed for each of
/// the 9 column and 30 row waves and builds two 8x30 grids of `POLY_FT4`s that
/// sample the two frame-buffer halves. Every later frame it ramps the
/// context's strength up to its target, or back down once the context asks
/// and then kills itself, advances the waves while a gameplay state flag is
/// clear, and draws the current buffer's grid with each vertex displaced by
/// sine waves scaled by that strength.
void func_shelter_b3_garbage_incinerator_8017E7D0(Task* arg0)
{
    ActorWaveScratch* scratch;
    ActorWaveScratch* head;
    ActorWaveCtx*     ctx;
    ActorWaveRec*     cols;
    POLY_FT4*         p;
    DR_STP*           stp;
    s32               i;
    s32               j;
    s32               k;
    s32               rowIndex;
    s32               rowBack;
    s32               u0;
    s32               u1;
    s32               v0;
    s32               v1;
    s32               waveX0;
    s32               waveY0;
    s32               waveX1;
    s32               waveY1;
    s32               waveX2;
    s32               waveY2;
    s32               waveX3;
    s32               waveY3;
    ActorWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                                = *(ActorWaveScratch**)G_SCRATCH_HEAD;
    D_800691CA                          = 2;
    *(ActorWaveScratch**)G_SCRATCH_HEAD = head - 1;
    cols                                = head[-1].cols;
    scratch                             = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_shelter_b3_garbage_incinerator_8018FC60[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FC60[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FC60[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b3_garbage_incinerator_8018FCB0[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_shelter_b3_garbage_incinerator_80185BC4          = 0;
            D_shelter_b3_garbage_incinerator_8018FC38          = arg0->spawnArg2;
            D_shelter_b3_garbage_incinerator_8018FC38->field_6 = 0;
            D_shelter_b3_garbage_incinerator_8018FC38->field_4 = 0;
            Display_ClampField126(-8);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = D_shelter_b3_garbage_incinerator_8018FEE0[i];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_shelter_b3_garbage_incinerator_8018FC38->field_8 == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_shelter_b3_garbage_incinerator_8018FC38->field_9;
                            p->g0 = D_shelter_b3_garbage_incinerator_8018FC38->field_A;
                            p->b0 = D_shelter_b3_garbage_incinerator_8018FC38->field_B;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b3_garbage_incinerator_8018FC38;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        if (D_801153F4 == 0) {
                            ctx->field_6--;
                        }
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_shelter_b3_garbage_incinerator_80185BC4 = D_shelter_b3_garbage_incinerator_8018FC38->field_6 * D_shelter_b3_garbage_incinerator_8018FC38->field_2 / D_shelter_b3_garbage_incinerator_8018FC38->field_0;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FC60[i].phase += D_shelter_b3_garbage_incinerator_8018FC60[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FC60[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FCB0[i].phase += D_shelter_b3_garbage_incinerator_8018FCB0[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FCB0[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = D_shelter_b3_garbage_incinerator_8018FEE0[D_80070F87];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        ActorWaveRec* next = row + 1;
                        waveX2             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2              = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2              = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3              = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3              = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
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
    *(ActorWaveScratch**)G_SCRATCH_HEAD += 1;
}
