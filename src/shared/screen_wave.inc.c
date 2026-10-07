/* Part of the screen wave library; see screen_wave.h. */

/// Redraws the captured frame as a fresh 10-by-30 mesh of textured quads each tick.
///
/// `task->spawnArg2.pointer` borrows a `ScreenWaveCtx` with positive `span`;
/// it must remain live through the task's final drawing tick. Only one wave
/// task may use a package's wave globals at a time. The first tick resets the
/// ramp to rising, seeds eleven column and thirty row waves and sets the
/// display's Y shake to -8 pixels; drawing starts on the next tick.
///
/// Ramp strength is `frame * scale / span`; its peak corresponds to
/// `scale / 32` pixels of sine displacement. X is signed, Y uses the absolute
/// displacement, and the top edge stays fixed. Waves advance every drawing
/// tick, including during actor freezes. The live context selects raw texture
/// colours or RGB modulation. A finished ramp kills the task and clears the
/// shake, but still draws that tick. Drawing consumes 300 `POLY_FT4` packets;
/// every tick also consumes two `DR_STP` packets to bracket mask-bit setting.
static void _screenWaveTask(Task* task)
{
    enum {
        SCREEN_WAVE_TASK_SEED           = 0,
        SCREEN_WAVE_TASK_DRAW           = 1,
        SCREEN_WAVE_PINNED_ROW          = -1,
        SCREEN_WAVE_QUAD_COLUMNS        = 10,
        SCREEN_WAVE_ROW_WAVES           = 30,
        SCREEN_WAVE_CELL_WIDTH          = 32,
        SCREEN_WAVE_CELL_HEIGHT         = 8,
        SCREEN_WAVE_CAPTURE_WIDTH       = 320,
        SCREEN_WAVE_RIGHT_PAGE_X        = 128,
        SCREEN_WAVE_FRAMEBUFFER_Y_SHIFT = 8,
        SCREEN_WAVE_BUFFER_V_OFFSET     = 16,
        SCREEN_WAVE_LEFT_X              = -160,
        SCREEN_WAVE_ROW_ZERO_Y          = -104,
        SCREEN_WAVE_PINNED_TOP_Y        = -112,
        SCREEN_WAVE_ROW_PHASE_SHIFT     = 9,
        SCREEN_WAVE_COLUMN_PHASE_SHIFT  = 10,
        SCREEN_WAVE_SINE_GAIN_SHIFT     = 3,
        SCREEN_WAVE_DISPLACEMENT_SHIFT  = 20,
        SCREEN_WAVE_TEXTURE_16_BIT      = 2,
        SCREEN_WAVE_QUAD_OT_INDEX       = 3,
        SCREEN_WAVE_MASK_SET_OT_INDEX   = 1023,
        SCREEN_WAVE_MASK_CLEAR_OT_INDEX = 0,
    };

    ScreenWaveCtx* rampContext;
    POLY_FT4*      quad;
    DR_STP*        maskCommand;
    s32            waveIndex, quadRow, quadColumn;
    s32            bufferVOffset;
    s32            leftTexturePage, rightTexturePage;
    s32            leftU, rightU, topV, bottomV;
    s32            waveX0, waveY0, waveX1, waveY1;
    s32            waveX2, waveY2, waveX3, waveY3;

    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    switch (task->state) {
        case SCREEN_WAVE_TASK_SEED:
            // Seed the ramp and independent waves; the first tick emits no quads.
            for (waveIndex = 0; waveIndex < SCREEN_WAVE_QUAD_COLUMNS + 1; waveIndex++) {
                gScreenWaveColumns[waveIndex].phase  = 0;
                gScreenWaveColumns[waveIndex].offset = (u32)rand() >> 3;
                gScreenWaveColumns[waveIndex].speed  = (rand() * 100 + 20) >> 15;
            }
            for (waveIndex = 0; waveIndex < SCREEN_WAVE_ROW_WAVES; waveIndex++) {
                gScreenWaveRows[waveIndex].phase  = 0;
                gScreenWaveRows[waveIndex].offset = (u32)rand() >> 3;
                gScreenWaveRows[waveIndex].speed  = (rand() * 100 + 20) >> 15;
            }
            gScreenWaveRamp       = 0;
            gScreenWaveCtx        = task->spawnArg2.pointer;
            gScreenWaveCtx->frame = 0;
            gScreenWaveCtx->state = SCREEN_WAVE_RAMP_RISING;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            task->state++;
            break;
        case SCREEN_WAVE_TASK_DRAW:
            rampContext = gScreenWaveCtx;
            switch (rampContext->state) {
                case SCREEN_WAVE_RAMP_RISING:
                    if (rampContext->frame < rampContext->span) {
                        rampContext->frame++;
                    }
                    break;
                case SCREEN_WAVE_RAMP_FALLING:
                    if (rampContext->frame > 0) {
                        rampContext->frame--;
                    } else {
                        rampContext->state = SCREEN_WAVE_RAMP_FINISHED;
                    }
                    break;
                case SCREEN_WAVE_RAMP_FINISHED:
                    taskKill(task);
                    displaySetShakeY(0);
                    break;
            }
            gScreenWaveRamp = gScreenWaveCtx->frame * gScreenWaveCtx->scale / gScreenWaveCtx->span;
            for (waveIndex = 0; waveIndex < SCREEN_WAVE_QUAD_COLUMNS + 1; waveIndex++) {
                gScreenWaveColumns[waveIndex].phase += gScreenWaveColumns[waveIndex].speed;
            }
            for (waveIndex = 0; waveIndex < SCREEN_WAVE_ROW_WAVES; waveIndex++) {
                gScreenWaveRows[waveIndex].phase += gScreenWaveRows[waveIndex].speed;
            }
            // Capture the two 16-bit texture pages of the current draw buffer.
            leftTexturePage  = getTPage(SCREEN_WAVE_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, 0, gDisplayState.drawBuffer << SCREEN_WAVE_FRAMEBUFFER_Y_SHIFT);
            rightTexturePage = getTPage(SCREEN_WAVE_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, SCREEN_WAVE_RIGHT_PAGE_X, gDisplayState.drawBuffer << SCREEN_WAVE_FRAMEBUFFER_Y_SHIFT);
            for (quadRow = SCREEN_WAVE_PINNED_ROW; quadRow < SCREEN_WAVE_ROW_WAVES - 1; quadRow++) {
                for (quadColumn = 0; quadColumn < SCREEN_WAVE_QUAD_COLUMNS; quadColumn++) {
                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    setPolyFT4(quad);
                    if (gScreenWaveCtx->modulateTexture == SCREEN_WAVE_TEXTURE_RAW) {
                        setShadeTex(quad, 1);
                    } else {
                        setShadeTex(quad, 0);
                        quad->r0 = gScreenWaveCtx->r;
                        quad->g0 = gScreenWaveCtx->g;
                        quad->b0 = gScreenWaveCtx->b;
                    }
                    leftU  = quadColumn * SCREEN_WAVE_CELL_WIDTH;
                    rightU = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH;
                    if (rightU == SCREEN_WAVE_CAPTURE_WIDTH)
                        rightU = SCREEN_WAVE_CAPTURE_WIDTH - 1;
                    if (leftU < SCREEN_WAVE_RIGHT_PAGE_X) {
                        quad->tpage = leftTexturePage;
                    } else {
                        quad->tpage = rightTexturePage;
                        leftU      -= SCREEN_WAVE_RIGHT_PAGE_X;
                        rightU     -= SCREEN_WAVE_RIGHT_PAGE_X;
                    }
                    if (quadRow != SCREEN_WAVE_PINNED_ROW) {
                        bottomV  = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + gDisplayState.drawBuffer * SCREEN_WAVE_BUFFER_V_OFFSET;
                        topV     = quadRow * SCREEN_WAVE_CELL_HEIGHT + gDisplayState.drawBuffer * SCREEN_WAVE_BUFFER_V_OFFSET;
                        waveX0   = gScreenWaveRamp * (rsin((quadRow << SCREEN_WAVE_ROW_PHASE_SHIFT) + gScreenWaveColumns[quadColumn].phase + gScreenWaveColumns[quadColumn].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x0 = quadColumn * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX0 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY0   = gScreenWaveRamp * (rsin((quadColumn << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + gScreenWaveRows[quadRow].phase + gScreenWaveRows[quadRow].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y0 = quadRow * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY0) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                        waveX1   = gScreenWaveRamp * (rsin((quadRow << SCREEN_WAVE_ROW_PHASE_SHIFT) + gScreenWaveColumns[quadColumn + 1].phase + gScreenWaveColumns[quadColumn + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x1 = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX1 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY1   = gScreenWaveRamp * (rsin(((quadColumn + 1) << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + gScreenWaveRows[quadRow].phase + gScreenWaveRows[quadRow].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y1 = quadRow * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY1) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                    } else {
                        // Mirror the first eight texture rows into the pinned top strip.
                        bufferVOffset = gDisplayState.drawBuffer * SCREEN_WAVE_BUFFER_V_OFFSET;
                        quad->x0      = quadColumn * SCREEN_WAVE_CELL_WIDTH + SCREEN_WAVE_LEFT_X;
                        quad->y0      = SCREEN_WAVE_PINNED_TOP_Y;
                        quad->x1      = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + SCREEN_WAVE_LEFT_X;
                        quad->y1      = SCREEN_WAVE_PINNED_TOP_Y;
                        topV          = bufferVOffset + SCREEN_WAVE_CELL_HEIGHT;
                        bottomV       = bufferVOffset;
                    }
                    waveX2   = gScreenWaveRamp * (rsin(((quadRow + 1) << SCREEN_WAVE_ROW_PHASE_SHIFT) + gScreenWaveColumns[quadColumn].phase + gScreenWaveColumns[quadColumn].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                    quad->x2 = quadColumn * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX2 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                    waveY2   = gScreenWaveRamp * (rsin((quadColumn << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + gScreenWaveRows[quadRow + 1].phase + gScreenWaveRows[quadRow + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                    quad->y2 = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY2) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                    waveX3   = gScreenWaveRamp * (rsin(((quadRow + 1) << SCREEN_WAVE_ROW_PHASE_SHIFT) + gScreenWaveColumns[quadColumn + 1].phase + gScreenWaveColumns[quadColumn + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                    quad->x3 = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX3 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                    waveY3   = gScreenWaveRamp * (rsin(((quadColumn + 1) << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + gScreenWaveRows[quadRow + 1].phase + gScreenWaveRows[quadRow + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                    quad->y3 = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY3) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                    setUV4(quad, leftU, topV, rightU, topV, leftU, bottomV, rightU, bottomV);
                    addPrim(&gGpuCurrentOt[SCREEN_WAVE_QUAD_OT_INDEX], quad);
                }
            }
            break;
    }
    // Enable mask-bit writes at the back of the ordering table, then clear them at the front.
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, 1);
    addPrim(&gGpuCurrentOt[SCREEN_WAVE_MASK_SET_OT_INDEX], maskCommand);
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, 0);
    addPrim(&gGpuCurrentOt[SCREEN_WAVE_MASK_CLEAR_OT_INDEX], maskCommand);
}
