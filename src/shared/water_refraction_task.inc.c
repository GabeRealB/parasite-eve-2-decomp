/* Part of the water effects library; see water_effects.h. */

/// Prepares screen-row rays and the intersection dividend for a horizontal water plane.
///
/// Borrows the reserved block and display state. `planeWorldY` uses whole world
/// units, while projection distance and row coordinates use pixels. Translation
/// narrows to signed halfwords before rotation through the transposed Q12 view
/// basis. The caller supplies each centred row Y, rotates that ray and divides
/// by its positive world Y component. Projection distance must fit a signed
/// halfword for the row ray; the signed plane-distance sum and product must
/// fit a word. Storage stays caller-owned. Overwrites GTE state; allocates nothing.
static inline void _waterPrepareRefractionProjection(WaterRefractionScratch* scratch, const DisplayState* display, s32 planeWorldY)
{
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->transposedView);
    scratch->viewTranslation.vx = gGfxViewCoord.workm.t[0];
    scratch->viewTranslation.vy = gGfxViewCoord.workm.t[1];
    scratch->viewTranslation.vz = gGfxViewCoord.workm.t[2];
    _gfxRotateSv(&scratch->transposedView, &scratch->viewTranslation);
    scratch->depth        = scratch->viewTranslation.vy + planeWorldY;
    scratch->depth       *= display->screenDistance;
    scratch->screenRow.vx = 0;
    scratch->screenRow.vz = display->screenDistance;
    gte_SetRotMatrix(&scratch->transposedView);
}

void waterRefractionTask(Task* task)
{
    enum {
        WATER_REFRACTION_CLIP_SPLIT                = 0,
        WATER_REFRACTION_CLIP_PAVILION_LEFT_FIRST  = 1,
        WATER_REFRACTION_CLIP_PAVILION_RIGHT_FIRST = 2,
        WATER_REFRACTION_CLIP_ISLAND_CENTRE_GAP    = 3,
        WATER_REFRACTION_CLIP_PAVILION_LEFT_EDGE   = 4,
        WATER_REFRACTION_PACKETS_PER_BUFFER        = 488,
        WATER_REFRACTION_TEXTURE_16_BIT            = 2,
        WATER_REFRACTION_TEXTURE_PAGE_Y_SHIFT      = 8,
        WATER_REFRACTION_TEXTURE_PAGE_RIGHT_X      = 128,
        WATER_REFRACTION_TEXTURE_U_RIGHT_BIAS      = 32,
        WATER_REFRACTION_TEXTURE_U_LEFT_BIAS       = 96,
        WATER_REFRACTION_BUFFER_V_GAP              = 16,
        WATER_REFRACTION_PACKET_WORDS              = (sizeof(POLY_FT4) - sizeof(u32)) / sizeof(u32),
        WATER_REFRACTION_RAW_FT4_CODE              = 0x2C | 0x01,
        WATER_REFRACTION_HALF_WIDTH                = 160,
        WATER_REFRACTION_HALF_HEIGHT               = 120,
        WATER_REFRACTION_FRAME_HEIGHT              = 240,
        WATER_REFRACTION_LAST_SAMPLE_ROW           = 239,
        WATER_REFRACTION_SAMPLE_REFLECTION_SUM     = 476,
        WATER_REFRACTION_SPLIT_AFTER_FRAME         = 1000,
        WATER_REFRACTION_WAVE_SCALE_ONE            = 1 << 12,
        WATER_REFRACTION_WAVE_SCALE_HALF           = 1 << 11,
        WATER_REFRACTION_WAVE_FRACTION_BITS        = 12,
        WATER_REFRACTION_WAVE_TO_PIXELS_SHIFT      = 9,
        WATER_REFRACTION_WAVE_DC_BIAS              = 2 << 12,
        WATER_REFRACTION_COSINE_PHASE_BIAS         = 308,
        WATER_REFRACTION_FRAME_PHASE_STEP          = 32,
        WATER_REFRACTION_ROW_SINE_STEP             = 31,
        WATER_REFRACTION_ROW_COSINE_STEP           = 197,
        WATER_REFRACTION_COSINE_DEPTH_START        = 768,
        WATER_REFRACTION_DEPTH_MASK                = 0x3FFF,
        WATER_REFRACTION_RAMP_ROWS                 = 8
    };
    s32                     sourceBufferIndex;
    s32                     sinePhase;
    s32                     cosinePhase;
    s32                     clipMode;
    s32                     waveScaleQ12;
    s32                     planeWorldY;
    s32                     splitRow;
    s32                     splitWidth;
    s32                     otIndexBias;
    s32                     baseLeftX;
    s32                     baseRightX;
    s32                     splitLeftX;
    s32                     firstRow;
    s32                     rowEnd;
    s32                     areaId;
    POLY_FT4*               strip;
    WaterRefractionScratch* scratch;
    s32                     rowY;
    s32                     centeredRowY;
    s32                     leftX;
    s32                     rightX;
    s32                     spanCount;
    s32                     spanIndex;
    s32                     fullWaveOffset;
    s32                     sineValue;
    s32                     cosineValue;
    s32                     waveOffset;
    s32                     rowsFromStart;
    s32                     quarterDepth;
    s32                     otIndex;
    s32                     sourceRow;
    s32                     rowsFromSplit;
    s32                     remainingSpanWidth;
    s32                     rightPageLeftX;
    s32                     leftPageRightX;
    s32                     screenLeftX;
    s32                     screenRightX;
    DisplayState*           display;

    // Select the water-plane height and pixel clipping for this room view.
    clipMode          = WATER_REFRACTION_CLIP_SPLIT;
    waveScaleQ12      = WATER_REFRACTION_WAVE_SCALE_ONE;
    planeWorldY       = 540;
    otIndexBias       = 0;
    baseLeftX         = -WATER_REFRACTION_HALF_WIDTH;
    baseRightX        = WATER_REFRACTION_HALF_WIDTH;
    splitLeftX        = -WATER_REFRACTION_HALF_WIDTH;
    splitRow          = 0;
    splitWidth        = 0;
    sourceBufferIndex = gDisplayState.otBuffer;
    areaId            = gGameSession->location.loc.area;
    if (areaId == GAME_AREA_NEO_ARK_BRIDGE) {
        otIndexBias = 10;
        switch (gGameSession->location.loc.view) {
            case 2:
                firstRow   = 127;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 159;
                splitWidth = 35;
                break;
            case 3:
                firstRow   = 74;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 104;
                splitWidth = 85;
                break;
            case 4:
                firstRow   = 1;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 116;
                splitWidth = -211;
                break;
            case 5:
                firstRow   = 102;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 107;
                splitWidth = 194;
                break;
            case 6:
                firstRow   = 147;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 161;
                splitWidth = 188;
                break;
            default:
                return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_ISLAND) {
        switch (gGameSession->location.loc.view) {
            case 2:
                firstRow   = 119;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 161;
                splitWidth = -44;
                break;
            case 3:
                firstRow   = 76;
                rowEnd     = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow   = 104;
                splitWidth = -78;
                break;
            case 4:
                firstRow     = 1;
                rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow     = WATER_REFRACTION_SPLIT_AFTER_FRAME;
                clipMode     = WATER_REFRACTION_CLIP_ISLAND_CENTRE_GAP;
                otIndexBias  = 10;
                waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
                break;
            default:
                return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_GARDEN) {
        if (gGameSession->location.loc.view == 2) {
            splitRow     = WATER_REFRACTION_SPLIT_AFTER_FRAME;
            firstRow     = 132;
            rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
            splitWidth   = 75;
            waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
        } else {
            return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_PAVILION) {
        otIndexBias = 10;
        switch (gGameSession->location.loc.view) {
            case 2:
            case 4:
                firstRow     = 82;
                rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
                clipMode     = WATER_REFRACTION_CLIP_PAVILION_LEFT_FIRST;
                splitRow     = WATER_REFRACTION_SPLIT_AFTER_FRAME;
                waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
                break;
            case 3:
            case 5:
                firstRow     = 76;
                rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
                clipMode     = WATER_REFRACTION_CLIP_PAVILION_RIGHT_FIRST;
                splitRow     = WATER_REFRACTION_SPLIT_AFTER_FRAME;
                waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
                break;
            case 6:
                firstRow     = 99;
                rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow     = 0;
                waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
                if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                    task->killCountdown += WATER_REFRACTION_FRAME_PHASE_STEP;
                }
                break;
            case 7:
                firstRow     = 53;
                rowEnd       = WATER_REFRACTION_FRAME_HEIGHT;
                clipMode     = WATER_REFRACTION_CLIP_PAVILION_LEFT_EDGE;
                waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
                if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                    task->killCountdown += WATER_REFRACTION_FRAME_PHASE_STEP;
                }
                break;
            default:
                return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY) {
        waveScaleQ12 = WATER_REFRACTION_WAVE_SCALE_HALF;
        otIndexBias  = -10;
        planeWorldY  = 4890;
        switch (gGameSession->location.loc.view) {
            case 2:
                baseLeftX  = 59;
                firstRow   = 169;
                rowEnd     = 224;
                splitRow   = -199;
                splitWidth = -67;
                break;
            case 3:
                otIndexBias = 10;
                baseLeftX   = -79;
                baseRightX  = 79;
                splitLeftX  = -58;
                firstRow    = 1;
                rowEnd      = 94;
                splitRow    = -73;
                splitWidth  = 120;
                break;
            case 4:
                baseRightX = -59;
                firstRow   = 169;
                rowEnd     = 224;
                splitRow   = -199;
                splitWidth = 67;
                break;
            case 5:
                firstRow    = 161;
                rowEnd      = WATER_REFRACTION_FRAME_HEIGHT;
                splitLeftX  = -138;
                splitWidth  = 276;
                splitRow    = 172;
                otIndexBias = 0;
                break;
            default:
                return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_WOODLAND_PATH) {
        planeWorldY = 140;
        switch (gGameSession->location.loc.view) {
            case 6:
                firstRow = 165;
                rowEnd   = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow = 0;
                break;
            case 7:
                firstRow = 164;
                rowEnd   = WATER_REFRACTION_FRAME_HEIGHT;
                splitRow = 0;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    screenLeftX  = -WATER_REFRACTION_HALF_WIDTH;
    screenRightX = WATER_REFRACTION_HALF_WIDTH;
    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }
    // The reserved actor-buffer prefix has one packet bank for each OT buffer.
    strip   = Fs_ActorLoadBase2;
    display = &gDisplayState;
    if (display->otBuffer != 0) {
        strip += WATER_REFRACTION_PACKETS_PER_BUFFER;
    }
    strip--;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        task->killCountdown += WATER_REFRACTION_FRAME_PHASE_STEP;
    }
    sinePhase   = task->killCountdown * 2;
    cosinePhase = task->killCountdown;
    SCRATCH_STACK_RESERVE_BLOCK(WaterRefractionScratch);
    scratch = SCRATCH_STACK_CURSOR(WaterRefractionScratch);
    // Express the view translation and row rays in the water plane's frame.
    _waterPrepareRefractionProjection(scratch, display, planeWorldY);

    for (rowY = firstRow; rowY < rowEnd; rowY++) {
        centeredRowY          = rowY - WATER_REFRACTION_HALF_HEIGHT;
        scratch->screenRow.vy = centeredRowY;
        gte_ldv0(&scratch->screenRow);
        gte_rtv0();
        leftX     = baseLeftX;
        rightX    = baseRightX;
        spanCount = 1;
        if (splitRow > 0) {
            if (rowY < splitRow + WATER_REFRACTION_RAMP_ROWS) {
                rightX = screenRightX;
                if (splitWidth > 0) {
                    leftX  = splitLeftX;
                    rightX = leftX + splitWidth;
                } else {
                    leftX = rightX + splitWidth;
                }
                if (splitRow < rowY) {
                    spanCount = 2;
                }
            }
        } else if (splitRow < 0 && -splitRow < rowY) {
            rightX = screenRightX;
            if (splitWidth > 0) {
                leftX  = splitLeftX;
                rightX = leftX + splitWidth;
            } else {
                leftX = rightX + splitWidth;
            }
        }
        // Convert the Q12 waves to pixel displacement and ramp the leading rows.
        sineValue    = rsin(sinePhase);
        cosineValue  = rcos(cosinePhase + WATER_REFRACTION_COSINE_PHASE_BIAS);
        sineValue   += WATER_REFRACTION_WAVE_DC_BIAS;
        waveOffset   = cosineValue + sineValue;
        waveOffset >>= WATER_REFRACTION_WAVE_TO_PIXELS_SHIFT;
        if (waveScaleQ12 != WATER_REFRACTION_WAVE_SCALE_ONE) {
            waveOffset     = (waveOffset * waveScaleQ12) >> WATER_REFRACTION_WAVE_FRACTION_BITS;
            fullWaveOffset = waveOffset;
        } else {
            fullWaveOffset = waveOffset;
        }
        waveOffset++;
        if (firstRow != 1) {
            rowsFromStart = rowY - firstRow;
            if (rowsFromStart < WATER_REFRACTION_RAMP_ROWS) {
                waveOffset  = fullWaveOffset >> ((WATER_REFRACTION_RAMP_ROWS - rowsFromStart) >> 1);
                waveOffset += 1;
            }
        }
        // Intersect the world-space row ray with the plane, then quantize its depth.
        gte_stsv(&scratch->rotatedRow);
        if (scratch->rotatedRow.vy > 0) {
            otIndex   = scratch->depth / scratch->rotatedRow.vy;
            otIndex >>= 2;
        } else {
            otIndex = WATER_REFRACTION_DEPTH_MASK;
        }
        sourceRow    = (centeredRowY + WATER_REFRACTION_HALF_HEIGHT) + waveOffset;
        quarterDepth = otIndex;
        otIndex      = ((quarterDepth << gDisplayState.otDepthShift) & WATER_REFRACTION_DEPTH_MASK) >> 4;
        otIndex     += otIndexBias;
        if (sourceRow >= WATER_REFRACTION_LAST_SAMPLE_ROW) {
            sourceRow = WATER_REFRACTION_SAMPLE_REFLECTION_SUM - sourceRow;
        }
        if (clipMode == WATER_REFRACTION_CLIP_PAVILION_LEFT_FIRST) {
            if (rowY < 125) {
                leftX  = -WATER_REFRACTION_HALF_WIDTH;
                rightX = WATER_REFRACTION_HALF_WIDTH;
            } else if (rowY < 179) {
                spanCount = 2;
                leftX     = -WATER_REFRACTION_HALF_WIDTH;
                rightX    = -89;
            } else {
                leftX  = -WATER_REFRACTION_HALF_WIDTH;
                rightX = -89;
            }
        } else if (clipMode == WATER_REFRACTION_CLIP_PAVILION_RIGHT_FIRST) {
            if (rowY < 131) {
                leftX  = -WATER_REFRACTION_HALF_WIDTH;
                rightX = WATER_REFRACTION_HALF_WIDTH;
            } else if (rowY < 183) {
                spanCount = 2;
                leftX     = 87;
                rightX    = WATER_REFRACTION_HALF_WIDTH;
            } else {
                leftX  = 87;
                rightX = WATER_REFRACTION_HALF_WIDTH;
            }
        } else if (clipMode == WATER_REFRACTION_CLIP_ISLAND_CENTRE_GAP) {
            if (rowY < 67) {
                spanCount = 1;
                leftX     = -WATER_REFRACTION_HALF_WIDTH;
                rightX    = WATER_REFRACTION_HALF_WIDTH;
            } else {
                spanCount = 2;
            }
        } else if (clipMode == WATER_REFRACTION_CLIP_PAVILION_LEFT_EDGE) {
            spanCount = 1;
            rightX    = WATER_REFRACTION_HALF_WIDTH;
            leftX     = -9;
            if (rowY >= 66) {
                leftX = -WATER_REFRACTION_HALF_WIDTH;
                if (rowY < 77) {
                    leftX = -106;
                }
            }
        }
        for (spanIndex = 0; spanIndex < spanCount; spanIndex++) {
            rowsFromSplit = rowY - splitRow;
            if (clipMode == WATER_REFRACTION_CLIP_PAVILION_LEFT_FIRST) {
                if (spanIndex != 0) {
                    leftX  = 60;
                    rightX = WATER_REFRACTION_HALF_WIDTH;
                }
            } else if (clipMode == WATER_REFRACTION_CLIP_PAVILION_RIGHT_FIRST) {
                if (spanIndex == 1) {
                    leftX  = -WATER_REFRACTION_HALF_WIDTH;
                    rightX = -105;
                }
            } else if (clipMode == WATER_REFRACTION_CLIP_ISLAND_CENTRE_GAP) {
                if (spanIndex == 0) {
                    if (rowY < 67) {
                        leftX  = -WATER_REFRACTION_HALF_WIDTH;
                        rightX = WATER_REFRACTION_HALF_WIDTH;
                    } else {
                        leftX  = -WATER_REFRACTION_HALF_WIDTH;
                        rightX = -87;
                    }
                } else {
                    if (rowY < 193) {
                        leftX  = 93;
                        rightX = WATER_REFRACTION_HALF_WIDTH;
                    } else {
                        leftX  = 42;
                        rightX = WATER_REFRACTION_HALF_WIDTH;
                    }
                }
            } else if (spanIndex == 1) {
                if (rowsFromSplit < WATER_REFRACTION_RAMP_ROWS) {
                    waveOffset = fullWaveOffset >> ((WATER_REFRACTION_RAMP_ROWS - rowsFromSplit) >> 1);
                    sourceRow  = WATER_REFRACTION_HALF_HEIGHT + 1;
                    sourceRow  = centeredRowY + (sourceRow + waveOffset);
                    if (sourceRow >= WATER_REFRACTION_LAST_SAMPLE_ROW) {
                        sourceRow = WATER_REFRACTION_SAMPLE_REFLECTION_SUM - sourceRow;
                    }
                }
                if (splitWidth > 0) {
                    remainingSpanWidth = splitWidth - WATER_REFRACTION_HALF_WIDTH * 2;
                } else {
                    remainingSpanWidth = splitWidth + WATER_REFRACTION_HALF_WIDTH * 2;
                }
                leftX = screenLeftX;
                if (remainingSpanWidth > 0) {
                    rightX = remainingSpanWidth + leftX;
                } else {
                    rightX = screenRightX;
                    leftX  = remainingSpanWidth + rightX;
                }
            }
            // Split each visible span at the two framebuffer texture pages.
            if (rightX > 0) {
                strip++;
                strip->y1      = centeredRowY;
                strip->y0      = centeredRowY;
                strip->y3      = centeredRowY + 1;
                strip->y2      = centeredRowY + 1;
                strip->tpage   = getTPage(WATER_REFRACTION_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, WATER_REFRACTION_TEXTURE_PAGE_RIGHT_X, sourceBufferIndex << WATER_REFRACTION_TEXTURE_PAGE_Y_SHIFT);
                rightPageLeftX = leftX;
                if (leftX < 0) {
                    rightPageLeftX = 0;
                }
                strip->x2 = rightPageLeftX;
                strip->x0 = rightPageLeftX;
                strip->u2 = rightPageLeftX + WATER_REFRACTION_TEXTURE_U_RIGHT_BIAS;
                strip->u0 = rightPageLeftX + WATER_REFRACTION_TEXTURE_U_RIGHT_BIAS;
                strip->x3 = rightX;
                strip->x1 = rightX;
                strip->u3 = rightX + WATER_REFRACTION_TEXTURE_U_RIGHT_BIAS;
                strip->u1 = rightX + WATER_REFRACTION_TEXTURE_U_RIGHT_BIAS;
                strip->v1 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP;
                strip->v0 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP;
                strip->v3 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP + 1;
                strip->v2 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP + 1;
                setlen(strip, WATER_REFRACTION_PACKET_WORDS);
                strip->code = WATER_REFRACTION_RAW_FT4_CODE;
                addPrim(&gGpuCurrentOt[otIndex], strip);
            }
            if (leftX <= 0) {
                strip++;
                strip->y1      = centeredRowY;
                strip->y0      = centeredRowY;
                strip->y3      = centeredRowY + 1;
                strip->y2      = centeredRowY + 1;
                strip->tpage   = getTPage(WATER_REFRACTION_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, 0, sourceBufferIndex << WATER_REFRACTION_TEXTURE_PAGE_Y_SHIFT);
                leftPageRightX = rightX;
                if (rightX > 0) {
                    leftPageRightX = 0;
                }
                strip->u2 = leftX - WATER_REFRACTION_TEXTURE_U_LEFT_BIAS;
                strip->u0 = leftX - WATER_REFRACTION_TEXTURE_U_LEFT_BIAS;
                strip->u3 = (leftX - WATER_REFRACTION_TEXTURE_U_LEFT_BIAS) + (leftPageRightX - leftX);
                strip->u1 = (leftX - WATER_REFRACTION_TEXTURE_U_LEFT_BIAS) + (leftPageRightX - leftX);
                strip->x2 = leftX;
                strip->x0 = leftX;
                strip->x3 = leftPageRightX;
                strip->x1 = leftPageRightX;
                strip->v1 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP;
                strip->v0 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP;
                strip->v3 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP + 1;
                strip->v2 = sourceRow + sourceBufferIndex * WATER_REFRACTION_BUFFER_V_GAP + 1;
                setlen(strip, WATER_REFRACTION_PACKET_WORDS);
                strip->code = WATER_REFRACTION_RAW_FT4_CODE;
                addPrim(&gGpuCurrentOt[otIndex], strip);
            }
        }
        sinePhase += WATER_REFRACTION_ROW_SINE_STEP;
        if (quarterDepth > WATER_REFRACTION_COSINE_DEPTH_START) {
            cosinePhase += WATER_REFRACTION_ROW_COSINE_STEP + (quarterDepth - WATER_REFRACTION_COSINE_DEPTH_START) / 4;
        } else {
            cosinePhase += WATER_REFRACTION_ROW_COSINE_STEP;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterRefractionScratch);
}
