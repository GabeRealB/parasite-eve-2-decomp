/* Part of the water effects library; see water_effects.h. */

/// Completes one raw framebuffer strip and links its caller-owned packet.
///
/// Screen X/Y, U coordinates and the texture page must already be initialized.
/// Source rows include the sixteen-texel framebuffer gap; `otIndex` selects a
/// valid current ordering-table tag. Packet storage must survive GPU drawing.
static inline void _waterFinishDistortionStrip(POLY_FT4* strip, s32 sourceRow, s32 sourceBufferIndex, s32 otIndex)
{
    enum {
        WATER_DISTORT_BUFFER_V_GAP = 16,
        WATER_DISTORT_PACKET_WORDS = sizeof(POLY_FT4) / sizeof(u32) - 1,
        WATER_DISTORT_RAW_FT4_CODE = 0x2C | 0x01,
    };
    strip->v1 = sourceRow + sourceBufferIndex * WATER_DISTORT_BUFFER_V_GAP;
    strip->v0 = sourceRow + sourceBufferIndex * WATER_DISTORT_BUFFER_V_GAP;
    strip->v3 = sourceRow + sourceBufferIndex * WATER_DISTORT_BUFFER_V_GAP + 1;
    strip->v2 = sourceRow + sourceBufferIndex * WATER_DISTORT_BUFFER_V_GAP + 1;
    setlen(strip, WATER_DISTORT_PACKET_WORDS);
    strip->code = WATER_DISTORT_RAW_FT4_CODE;
    addPrim(&gGpuCurrentOt[otIndex], strip);
}

void waterDistortBandTask(Task* task)
{
    enum {
        WATER_DISTORT_PACKETS_PER_BUFFER        = 488,
        WATER_DISTORT_REFRACTION_PREFIX_PACKETS = 976,
        WATER_DISTORT_ACTOR_ARENA_BYTES         = 0x30000,
        WATER_DISTORT_SCRATCH_BYTES             = 0x40,
        WATER_DISTORT_HALF_WIDTH                = 160,
        WATER_DISTORT_HALF_HEIGHT               = 120,
        WATER_DISTORT_LAST_SAMPLE_ROW           = 239,
        WATER_DISTORT_SAMPLE_REFLECTION_SUM     = 476,
        WATER_DISTORT_DEPTH_MASK                = 0x3FFF,
        WATER_DISTORT_FADE_ROWS                 = 16,
        WATER_DISTORT_FADE_SHIFT                = 4,
        WATER_DISTORT_WAVE_TO_PIXELS_SHIFT      = 10,
        WATER_DISTORT_WAVE_DC_BIAS              = 2 << 12,
        WATER_DISTORT_COSINE_PHASE_BIAS         = 308,
        WATER_DISTORT_SINE_FRAME_SHIFT          = 5,
        WATER_DISTORT_COSINE_FRAME_SHIFT        = 4,
        WATER_DISTORT_SINE_ROW_STEP             = 31,
        WATER_DISTORT_COSINE_ROW_STEP           = 197,
        WATER_DISTORT_TEXTURE_16_BIT            = 2,
        WATER_DISTORT_TEXTURE_PAGE_Y_SHIFT      = 8,
        WATER_DISTORT_TEXTURE_RIGHT_PAGE_X      = 128,
        WATER_DISTORT_TEXTURE_U_RIGHT_BIAS      = 32,
        WATER_DISTORT_TEXTURE_U_LEFT_BIAS       = 96,
    };
    s32              xLeft             = -WATER_DISTORT_HALF_WIDTH;
    s32              xRight            = WATER_DISTORT_HALF_WIDTH;
    s32              sourceBufferIndex = gDisplayState.otBuffer;
    s32              bandCount         = 1;
    GameLocationKey* location          = &gGameSession->location.loc;
    s32              areaId            = location->area;
    s32              firstRow;
    s32              rowEnd;
    POLY_FT4*        strip;
    u8*              packetBytes;
    s32              freeBytes;
    s32              sinePhase;
    s32              cosinePhase;
    s32              otIndex;
    s32              bandIndex;
    s32              rowY;
    s32              centeredRowY;
    s32              waveOffset;
    s32              sineValue;
    s32              cosineValue;
    s32              sourceRow;
    s32              rightPageLeftX;
    s32              leftPageRightX;
    u16              unusedFrameSlot;

    // Select screen-pixel clipping for the current room view.
    if (areaId == GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL) {
        switch (gGameSession->location.loc.view) {
            case 2:
                firstRow = 1;
                rowEnd   = 0x3F;
                break;
            case 3:
                firstRow = 1;
                rowEnd   = 0x49;
                break;
            case 4:
                firstRow = 1;
                rowEnd   = 0x49;
                break;
            case 5:
                firstRow = 1;
                rowEnd   = 0x3F;
                break;
            case 8:
                firstRow = 1;
                rowEnd   = 0x72;
                break;
            case 9:
                firstRow = 1;
                rowEnd   = 0x45;
                break;
            default:
                return;
        }
    } else if (areaId == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY) {
        switch (gGameSession->location.loc.view) {
            case 2:
                firstRow = 1;
                rowEnd   = 0x40;
                xRight   = -0x50;
                break;
            case 4:
                firstRow = 0x3B;
                rowEnd   = 0x55;
                xRight   = -0x67;
                break;
            case 5:
                firstRow  = 0x22;
                rowEnd    = 0x4A;
                xRight    = -0x4E;
                bandCount = 2;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }

    // Variant 3 uses the unloaded tail of actor arena 0; other views follow
    // the two refraction banks in actor arena 2. The dynamic split is in bytes.
    if (areaId == GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL && location->variant == 3) {
        freeBytes   = WATER_DISTORT_ACTOR_ARENA_BYTES - Fs_ChunkOutputSizes[0];
        freeBytes  &= ~7;
        packetBytes = (u8*)Fs_ActorLoadBase0 - (freeBytes - WATER_DISTORT_ACTOR_ARENA_BYTES);
        if (freeBytes < sizeof(POLY_FT4) * WATER_DISTORT_PACKETS_PER_BUFFER * 2) {
            return;
        }
        if (gDisplayState.otBuffer != 0) {
            packetBytes += freeBytes >> 1;
        }
        strip = (POLY_FT4*)packetBytes - 1;
    } else {
        strip = (POLY_FT4*)Fs_ActorLoadBase2 + WATER_DISTORT_REFRACTION_PREFIX_PACKETS;
        if (gDisplayState.otBuffer != 0) {
            strip += WATER_DISTORT_PACKETS_PER_BUFFER;
        }
        strip--;
    }

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        task->killCountdown++;
    }
    sinePhase   = task->killCountdown << WATER_DISTORT_SINE_FRAME_SHIFT;
    cosinePhase = task->killCountdown << WATER_DISTORT_COSINE_FRAME_SHIFT;
    SCRATCH_STACK_RESERVE_BYTES(WATER_DISTORT_SCRATCH_BYTES);
    otIndex = ((WATER_DISTORT_DEPTH_MASK << gDisplayState.otDepthShift) & WATER_DISTORT_DEPTH_MASK) >> 4;

    for (bandIndex = 0; bandIndex < bandCount; bandIndex++) {
        if (bandIndex == 1) {
            firstRow = 0x2E;
            rowEnd   = 0x54;
            xLeft    = 1;
            xRight   = 0x55;
        }
        // Fade the vertical sampling displacement over the final sixteen rows.
        for (rowY = firstRow; rowY < rowEnd; rowY++) {
            centeredRowY = rowY - WATER_DISTORT_HALF_HEIGHT;
            sineValue    = rsin(sinePhase);
            cosineValue  = rcos(cosinePhase + WATER_DISTORT_COSINE_PHASE_BIAS);
            sineValue   += WATER_DISTORT_WAVE_DC_BIAS;
            waveOffset   = cosineValue + sineValue;
            waveOffset >>= WATER_DISTORT_WAVE_TO_PIXELS_SHIFT;
            if (rowEnd - WATER_DISTORT_FADE_ROWS < rowY) {
                waveOffset = (waveOffset * (rowEnd - rowY)) >> WATER_DISTORT_FADE_SHIFT;
            }
            cosineValue = waveOffset + WATER_DISTORT_HALF_HEIGHT + 1;
            sourceRow   = centeredRowY + cosineValue;
            otIndex--;
            if (sourceRow >= WATER_DISTORT_LAST_SAMPLE_ROW) {
                sourceRow = WATER_DISTORT_SAMPLE_REFLECTION_SUM - sourceRow;
            }
            if (sourceRow < 0) {
                sourceRow = -sourceRow;
            }
            // Split the row at the two framebuffer texture pages.
            if (xRight > 0) {
                rightPageLeftX = xLeft < 0 ? 0 : xLeft;
                strip++;
                strip->y1    = centeredRowY;
                strip->y0    = centeredRowY;
                strip->y3    = rowY - (WATER_DISTORT_HALF_HEIGHT - 1);
                strip->y2    = rowY - (WATER_DISTORT_HALF_HEIGHT - 1);
                strip->tpage = getTPage(WATER_DISTORT_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, WATER_DISTORT_TEXTURE_RIGHT_PAGE_X, sourceBufferIndex << WATER_DISTORT_TEXTURE_PAGE_Y_SHIFT);
                strip->x2    = rightPageLeftX;
                strip->x0    = rightPageLeftX;
                strip->u2    = rightPageLeftX + WATER_DISTORT_TEXTURE_U_RIGHT_BIAS;
                strip->u0    = rightPageLeftX + WATER_DISTORT_TEXTURE_U_RIGHT_BIAS;
                strip->x3    = xRight;
                strip->x1    = xRight;
                strip->u3    = xRight + WATER_DISTORT_TEXTURE_U_RIGHT_BIAS;
                strip->u1    = xRight + WATER_DISTORT_TEXTURE_U_RIGHT_BIAS;
                _waterFinishDistortionStrip(strip, sourceRow, sourceBufferIndex, otIndex);
            }
            if (xLeft < 0) {
                leftPageRightX = xRight;
                if (leftPageRightX > 0) {
                    leftPageRightX = 0;
                }
                strip++;
                strip->y1    = centeredRowY;
                strip->y0    = centeredRowY;
                strip->y3    = rowY - (WATER_DISTORT_HALF_HEIGHT - 1);
                strip->y2    = rowY - (WATER_DISTORT_HALF_HEIGHT - 1);
                strip->tpage = getTPage(WATER_DISTORT_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, 0, sourceBufferIndex << WATER_DISTORT_TEXTURE_PAGE_Y_SHIFT);
                strip->u2    = xLeft - WATER_DISTORT_TEXTURE_U_LEFT_BIAS;
                strip->u0    = xLeft - WATER_DISTORT_TEXTURE_U_LEFT_BIAS;
                strip->u3    = leftPageRightX - WATER_DISTORT_TEXTURE_U_LEFT_BIAS;
                strip->u1    = leftPageRightX - WATER_DISTORT_TEXTURE_U_LEFT_BIAS;
                strip->x2    = xLeft;
                strip->x0    = xLeft;
                strip->x3    = leftPageRightX;
                strip->x1    = leftPageRightX;
                _waterFinishDistortionStrip(strip, sourceRow, sourceBufferIndex, otIndex);
            }
            // The compiler folds this pre-existing use; removing it shifts retail spill offsets.
            sinePhase   += WATER_DISTORT_SINE_ROW_STEP + (unusedFrameSlot >> 16);
            cosinePhase += WATER_DISTORT_COSINE_ROW_STEP;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(WATER_DISTORT_SCRATCH_BYTES);
}
