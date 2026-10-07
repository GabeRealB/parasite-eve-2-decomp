/* Part of the screen wave library; see screen_wave.h. */

/// Scratch-stack block the grid task holds for the length of one tick: the
/// frame's snapshot of every wave the vertex pass reads.
///
/// Only the `phase` and `offset` of each record are filled, copied from the
/// package's `gScreenWaveRows` and `gScreenWaveColumns` after they advance;
/// `speed` and `field_6` are left as the scratch pad had them and never read.
/// The mesh is 8 quads wide and 30 tall. Its nine vertical edges each have a
/// wave; its top edge is pinned, so the 30 horizontal edges below it do.
typedef struct {
    ScreenWaveGridOscillator rows[30];   // Wave of each horizontal grid edge, top to bottom
    ScreenWaveGridOscillator columns[9]; // Wave of each vertical grid edge, left to right
} _ScreenWaveGridScratch;
STATIC_ASSERT_SIZEOF(_ScreenWaveGridScratch, 0x138);

/// Redraws the captured frame through a persistent, double-buffered 8-by-30 mesh.
///
/// `task->spawnArg2.pointer` borrows a `ScreenWaveCtx` with positive `span`;
/// it must remain live through the task's final drawing tick. Only one wave
/// task may use a package's wave globals at a time. The first tick resets the
/// ramp to rising, seeds nine column and thirty row waves, sets the display's
/// Y shake to -8 pixels, and fixes each buffer's UVs and optional RGB tint.
/// Later ticks move the corners and submit the current buffer's 240 quads.
///
/// Ramp strength is `frame * scale / span`; its peak corresponds to
/// `scale / 32` pixels of sine displacement. X is signed, Y uses the absolute
/// displacement, and the top edge stays fixed. Actor freezes stop phase
/// advancement and a falling ramp, while a rising ramp continues. A finished
/// ramp kills the task and clears the shake, but still draws that tick.
/// Each tick borrows and releases one 0x138-byte scratch-stack snapshot and
/// brackets the ordering table with mask-bit setting commands.
static void _screenWaveGridTask(Task* task)
{
    enum {
        SCREEN_WAVE_TASK_SEED           = 0,
        SCREEN_WAVE_TASK_DRAW           = 1,
        SCREEN_WAVE_PINNED_ROW          = -1,
        SCREEN_WAVE_QUAD_COLUMNS        = ARRAY_SIZE(SCREEN_WAVE_GRID[0][0]),
        SCREEN_WAVE_ROW_WAVES           = ARRAY_SIZE(SCREEN_WAVE_GRID[0]),
        SCREEN_WAVE_CELL_WIDTH          = 40,
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

    _ScreenWaveGridScratch*   waveSnapshot;
    _ScreenWaveGridScratch*   scratchTop;
    ScreenWaveCtx*            rampContext;
    ScreenWaveGridOscillator* columnWaves;
    POLY_FT4*                 quad;
    DR_STP*                   maskCommand;
    s32                       index;
    s32                       quadRow;
    s32                       quadColumn;
    s32                       waveRow;
    s32                       reverseWaveRow;
    s32                       leftU;
    s32                       rightU;
    s32                       topV;
    s32                       bottomV;
    s32                       waveX0;
    s32                       waveY0;
    s32                       waveX1;
    s32                       waveY1;
    s32                       waveX2;
    s32                       waveY2;
    s32                       waveX3;
    s32                       waveY3;
    ScreenWaveGridOscillator* rowWave;
    POLY_FT4(*meshRows)
    [SCREEN_WAVE_QUAD_COLUMNS];
    s32 leftTexturePage;
    s32 rightTexturePage;

    scratchTop                                   = SCRATCH_STACK_CURSOR(_ScreenWaveGridScratch);
    gCdCmdQueue.imageMdecMode                    = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    SCRATCH_STACK_CURSOR(_ScreenWaveGridScratch) = scratchTop - 1;
    columnWaves                                  = scratchTop[-1].columns;
    waveSnapshot                                 = scratchTop - 1;
    switch (task->state) {
        case SCREEN_WAVE_TASK_SEED:
            // Seed the ramp and build the UVs and tint once for both buffers.
            for (index = 0; index < SCREEN_WAVE_QUAD_COLUMNS + 1; index++) {
                gScreenWaveColumns[index].phase  = 0;
                gScreenWaveColumns[index].offset = (u32)rand() >> 3;
                gScreenWaveColumns[index].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (index = 0; index < SCREEN_WAVE_ROW_WAVES; index++) {
                gScreenWaveRows[index].phase  = 0;
                gScreenWaveRows[index].offset = (u32)rand() >> 3;
                gScreenWaveRows[index].speed  = ((rand() * 100) >> 15) + 20;
            }
            gScreenWaveRamp       = 0;
            gScreenWaveCtx        = task->spawnArg2.pointer;
            gScreenWaveCtx->frame = 0;
            gScreenWaveCtx->state = SCREEN_WAVE_RAMP_RISING;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            for (index = 0; index < ARRAY_SIZE(SCREEN_WAVE_GRID); index++) {
                leftTexturePage  = getTPage(SCREEN_WAVE_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, 0, index << SCREEN_WAVE_FRAMEBUFFER_Y_SHIFT);
                rightTexturePage = getTPage(SCREEN_WAVE_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, SCREEN_WAVE_RIGHT_PAGE_X, index << SCREEN_WAVE_FRAMEBUFFER_Y_SHIFT);
                // Bias the mesh by one row so logical row -1 is its pinned top strip.
                meshRows = &SCREEN_WAVE_GRID[index][1];
                for (quadRow = SCREEN_WAVE_PINNED_ROW; quadRow < SCREEN_WAVE_ROW_WAVES - 1; quadRow++) {
                    quad = meshRows[quadRow];
                    for (quadColumn = 0; quadColumn < SCREEN_WAVE_QUAD_COLUMNS; quad++, quadColumn++) {
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
                        if (rightU == SCREEN_WAVE_CAPTURE_WIDTH) {
                            rightU = SCREEN_WAVE_CAPTURE_WIDTH - 1;
                        }
                        if (leftU < SCREEN_WAVE_RIGHT_PAGE_X) {
                            quad->tpage = leftTexturePage;
                        } else {
                            quad->tpage = rightTexturePage;
                            leftU      -= SCREEN_WAVE_RIGHT_PAGE_X;
                            rightU     -= SCREEN_WAVE_RIGHT_PAGE_X;
                        }
                        bottomV = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + index * SCREEN_WAVE_BUFFER_V_OFFSET;
                        if (quadRow != SCREEN_WAVE_PINNED_ROW) {
                            topV = quadRow * SCREEN_WAVE_CELL_HEIGHT + index * SCREEN_WAVE_BUFFER_V_OFFSET;
                        } else {
                            topV    = index * SCREEN_WAVE_BUFFER_V_OFFSET + SCREEN_WAVE_CELL_HEIGHT;
                            bottomV = index * SCREEN_WAVE_BUFFER_V_OFFSET;
                        }
                        quad->u0 = leftU;
                        quad->v0 = topV;
                        quad->u1 = rightU;
                        quad->v1 = topV;
                        do {
                            quad->u2 = leftU;
                            quad->v2 = bottomV;
                            quad->u3 = rightU;
                        } while (0);
                        quad->v3 = bottomV;
                    }
                }
            }
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
                        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                            rampContext->frame--;
                        }
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
            // Snapshot only the phase/offset word; speed and the unknown tail
            // stay untouched. The globals and scratch entries are word-aligned.
            for (index = 0; index < ARRAY_SIZE(waveSnapshot->columns); index++) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    gScreenWaveColumns[index].phase += gScreenWaveColumns[index].speed;
                }
                *(s32*)&columnWaves[index] = *(s32*)&gScreenWaveColumns[index];
            }
            for (index = 0; index < ARRAY_SIZE(waveSnapshot->rows); index++) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    gScreenWaveRows[index].phase += gScreenWaveRows[index].speed;
                }
                *(s32*)&waveSnapshot->rows[index] = *(s32*)&gScreenWaveRows[index];
            }
            // Displace this buffer's corners; only the top strip lacks a top-edge wave.
            waveRow = SCREEN_WAVE_PINNED_ROW;
            for (quadRow = SCREEN_WAVE_PINNED_ROW; quadRow < SCREEN_WAVE_ROW_WAVES - 1; waveRow += 2, quadRow++, waveRow--) {
                reverseWaveRow = -waveRow;
                rowWave        = waveSnapshot->rows - reverseWaveRow;
                meshRows       = &SCREEN_WAVE_GRID[gDisplayState.drawBuffer][1];
                quad           = meshRows[quadRow];
                for (quadColumn = 0; quadColumn < SCREEN_WAVE_QUAD_COLUMNS; quadColumn++, quad++) {
                    if (quadRow != SCREEN_WAVE_PINNED_ROW) {
                        waveX0   = gScreenWaveRamp * (rsin((quadRow << SCREEN_WAVE_ROW_PHASE_SHIFT) + columnWaves[quadColumn].phase + columnWaves[quadColumn].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x0 = quadColumn * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX0 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY0   = gScreenWaveRamp * (rsin((quadColumn << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + rowWave->phase + rowWave->offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y0 = quadRow * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY0) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                        waveX1   = gScreenWaveRamp * (rsin((quadRow << SCREEN_WAVE_ROW_PHASE_SHIFT) + columnWaves[quadColumn + 1].phase + columnWaves[quadColumn + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x1 = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX1 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY1   = gScreenWaveRamp * (rsin(((quadColumn + 1) << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + rowWave->phase + rowWave->offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y1 = quadRow * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY1) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                    } else {
                        quad->x0 = quadColumn * SCREEN_WAVE_CELL_WIDTH + SCREEN_WAVE_LEFT_X;
                        quad->y0 = SCREEN_WAVE_PINNED_TOP_Y;
                        quad->x1 = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + SCREEN_WAVE_LEFT_X;
                        quad->y1 = SCREEN_WAVE_PINNED_TOP_Y;
                    }
                    {
                        ScreenWaveGridOscillator* nextRowWave = rowWave + 1;
                        waveX2                                = gScreenWaveRamp * (rsin(((quadRow + 1) << SCREEN_WAVE_ROW_PHASE_SHIFT) + columnWaves[quadColumn].phase + columnWaves[quadColumn].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x2                              = quadColumn * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX2 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY2                                = gScreenWaveRamp * (rsin((quadColumn << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + rowWave[1].phase + nextRowWave->offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y2                              = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY2) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                        waveX3                                = gScreenWaveRamp * (rsin(((quadRow + 1) << SCREEN_WAVE_ROW_PHASE_SHIFT) + columnWaves[quadColumn + 1].phase + columnWaves[quadColumn + 1].offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->x3                              = (quadColumn + 1) * SCREEN_WAVE_CELL_WIDTH + (s16)((waveX3 >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_LEFT_X);
                        waveY3                                = gScreenWaveRamp * (rsin(((quadColumn + 1) << SCREEN_WAVE_COLUMN_PHASE_SHIFT) + rowWave[1].phase + nextRowWave->offset) << SCREEN_WAVE_SINE_GAIN_SHIFT);
                        quad->y3                              = (quadRow + 1) * SCREEN_WAVE_CELL_HEIGHT + (s16)((ABS(waveY3) >> SCREEN_WAVE_DISPLACEMENT_SHIFT) + SCREEN_WAVE_ROW_ZERO_Y);
                    }
                    addPrim(&gGpuCurrentOt[SCREEN_WAVE_QUAD_OT_INDEX], quad);
                }
                SOFT_USE_REG(quad);
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
    SCRATCH_STACK_RELEASE_BLOCK(_ScreenWaveGridScratch);
}
