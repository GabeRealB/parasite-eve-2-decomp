/* Part of the screen wave library; see screen_wave.h. */

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `displaySetShakeY`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front.
void screenWaveTask(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;
    s32*            state;

    CdCmd_Queue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    /* Through a pointer rather than as `arg0->state`: a member load is struct
       memory, which the scheduler lets rise above the store before it, and the
       original keeps the two in source order. */
    state = &arg0->state;
    switch (*state) {
        case 0:
            for (i = 0; i < 11; i++) {
                gScreenWaveColumns[i].phase  = 0;
                gScreenWaveColumns[i].offset = (u32)rand() >> 3;
                gScreenWaveColumns[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                gScreenWaveRows[i].phase  = 0;
                gScreenWaveRows[i].offset = (u32)rand() >> 3;
                gScreenWaveRows[i].speed  = (rand() * 100 + 20) >> 15;
            }
            gScreenWaveRamp       = 0;
            gScreenWaveCtx        = arg0->spawnArg2.pointer;
            gScreenWaveCtx->frame = 0;
            gScreenWaveCtx->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
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
                        ctx->frame--;
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
            for (i = 0; i < 11; i++) {
                gScreenWaveColumns[i].phase += gScreenWaveColumns[i].speed;
            }
            for (i = 0; i < 30; i++) {
                gScreenWaveRows[i].phase += gScreenWaveRows[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = gGpuPrimCursor;
                    gGpuPrimCursor = p + 1;
                    setPolyFT4(p);
                    if (gScreenWaveCtx->blend == ANIMATION_BLEND_RESET) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = gScreenWaveCtx->r;
                        p->g0 = gScreenWaveCtx->g;
                        p->b0 = gScreenWaveCtx->b;
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
                        waveX0 = gScreenWaveRamp * (rsin((j << 9) + gScreenWaveColumns[k].phase + gScreenWaveColumns[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = gScreenWaveRamp * (rsin((k << 10) + gScreenWaveRows[j].phase + gScreenWaveRows[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = gScreenWaveRamp * (rsin((j << 9) + gScreenWaveColumns[k + 1].phase + gScreenWaveColumns[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = gScreenWaveRamp * (rsin(((k + 1) << 10) + gScreenWaveRows[j].phase + gScreenWaveRows[j].offset) << 3);
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

                        waveX2 = gScreenWaveRamp * (rsin(((j + 1) << 9) + gScreenWaveColumns[k].phase + gScreenWaveColumns[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = gScreenWaveRamp * (rsin((k << 10) + gScreenWaveRows[j + 1].phase + gScreenWaveRows[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = gScreenWaveRamp * (rsin(((j + 1) << 9) + gScreenWaveColumns[k + 1].phase + gScreenWaveColumns[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = gScreenWaveRamp * (rsin(((k + 1) << 10) + gScreenWaveRows[j + 1].phase + gScreenWaveRows[j + 1].offset) << 3);
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
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}
