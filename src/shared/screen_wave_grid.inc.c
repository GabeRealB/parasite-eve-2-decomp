/* Part of the screen wave library; see screen_wave.h. */

/// Screen-wave task, spawned through `D_actor_205200_8014CA44` with the
/// context `func_actor_205200_8014AB98` fills. State 0 seeds random phases and
/// speeds for the 9 column and 30 row waves and builds, for each display
/// buffer, a grid of textured quads that re-draws the frame buffer. State 1
/// ramps the amplitude up to the context's peak, back down once its mode turns
/// to 1, and kills the task at mode 2; each frame it displaces every quad
/// vertex by the sine of its row and column waves.
void screenWaveGridTask(Task* arg0)
{
    OverlayWaveScratch* scratch;
    OverlayWaveScratch* head;
    OverlayWaveCtx*     ctx;
    OverlayWaveRec*     cols;
    POLY_FT4*           p;
    DR_STP*             stp;
    s32                 i;
    s32                 j;
    s32                 k;
    s32                 rowIndex;
    s32                 rowBack;
    s32                 u0;
    s32                 u1;
    s32                 v0;
    s32                 v1;
    s32                 waveX0;
    s32                 waveY0;
    s32                 waveX1;
    s32                 waveY1;
    s32                 waveX2;
    s32                 waveY2;
    s32                 waveX3;
    s32                 waveY3;
    OverlayWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                                     = SCRATCH_STACK_CURSOR(OverlayWaveScratch);
    gCdCmdQueue.imageMdecMode                = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    SCRATCH_STACK_CURSOR(OverlayWaveScratch) = head - 1;
    cols                                     = head[-1].cols;
    scratch                                  = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                gScreenWaveColumns[i].phase  = 0;
                gScreenWaveColumns[i].offset = (u32)rand() >> 3;
                gScreenWaveColumns[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                gScreenWaveRows[i].phase  = 0;
                gScreenWaveRows[i].offset = (u32)rand() >> 3;
                gScreenWaveRows[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            gScreenWaveRamp       = 0;
            gScreenWaveCtx        = arg0->spawnArg2.pointer;
            gScreenWaveCtx->frame = 0;
            gScreenWaveCtx->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = &SCREEN_WAVE_GRID[i][1];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (gScreenWaveCtx->blend == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = gScreenWaveCtx->r;
                            p->g0 = gScreenWaveCtx->g;
                            p->b0 = gScreenWaveCtx->b;
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
            ctx = gScreenWaveCtx;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        if (Gp_StateF0.field_4 == 0) {
                            ctx->frame--;
                        }
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    displaySetShakeY(0);
                    break;
            }
            gScreenWaveRamp = gScreenWaveCtx->frame * gScreenWaveCtx->scale / gScreenWaveCtx->span;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    gScreenWaveColumns[i].phase += gScreenWaveColumns[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&gScreenWaveColumns[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    gScreenWaveRows[i].phase += gScreenWaveRows[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&gScreenWaveRows[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = &SCREEN_WAVE_GRID[gDisplayState.drawBuffer][1];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = gScreenWaveRamp * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = gScreenWaveRamp * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = gScreenWaveRamp * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = gScreenWaveRamp * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        OverlayWaveRec* next = row + 1;
                        waveX2               = gScreenWaveRamp * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2                = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2               = gScreenWaveRamp * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2                = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3               = gScreenWaveRamp * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3                = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3               = gScreenWaveRamp * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3                = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    SCRATCH_STACK_RELEASE_BLOCK(OverlayWaveScratch);
}
