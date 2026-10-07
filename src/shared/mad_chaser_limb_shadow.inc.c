/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Queues one accepted Mad Chaser shadow projection with subtractive blending.
///
/// Requires a nonnegative projection FLAG. shade is grey texture modulation
/// (0..255); depth is the projection sorting Z/4, wrapped to one of 1024 tags.
/// Borrows the scratch result only during this call. The frame arena must have
/// room for one word-aligned POLY_FT4 and the current ordering table must have
/// 1024 depth tags. The packet remains live until frame DMA completes.
static __inline__ void _madChaserQueueLimbShadow(const MadChaserLimbShadowScratch* scratch, u8 shade)
{
    enum {
        MAD_CHASER_SHADOW_TEXTURE_4_BIT = 0,
        MAD_CHASER_SHADOW_U_MIN         = 0xC0,
        MAD_CHASER_SHADOW_U_MAX         = 0xF7,
        MAD_CHASER_SHADOW_V_MIN         = 0x98,
        MAD_CHASER_SHADOW_V_MAX         = 0xCF,
        MAD_CHASER_SHADOW_PAGE_X        = 512,
        MAD_CHASER_SHADOW_PAGE_Y        = 0,
        MAD_CHASER_SHADOW_CLUT_X        = 48,
        MAD_CHASER_SHADOW_CLUT_Y        = 266,
    };
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setSemiTrans(quad, true);
    GPU_PRIMITIVE_XY_WORD(quad, 0) = scratch->screenCorners[0];
    GPU_PRIMITIVE_XY_WORD(quad, 1) = scratch->screenCorners[1];
    GPU_PRIMITIVE_XY_WORD(quad, 2) = scratch->screenCorners[2];
    GPU_PRIMITIVE_XY_WORD(quad, 3) = scratch->screenCorners[3];
    setUV4(quad, MAD_CHASER_SHADOW_U_MIN, MAD_CHASER_SHADOW_V_MIN, MAD_CHASER_SHADOW_U_MAX, MAD_CHASER_SHADOW_V_MIN,
           MAD_CHASER_SHADOW_U_MIN, MAD_CHASER_SHADOW_V_MAX, MAD_CHASER_SHADOW_U_MAX, MAD_CHASER_SHADOW_V_MAX);
    quad->tpage = getTPage(MAD_CHASER_SHADOW_TEXTURE_4_BIT, GPU_BLEND_SUBTRACT, MAD_CHASER_SHADOW_PAGE_X, MAD_CHASER_SHADOW_PAGE_Y);
    quad->clut  = getClut(MAD_CHASER_SHADOW_CLUT_X, MAD_CHASER_SHADOW_CLUT_Y);
    setRGB0(quad, shade, shade, shade);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), quad);
}

/// Draws a horizontal subtractive limb shadow between two model coordinates.
///
/// firstJoint and secondJoint must be in 0..partCount-1 of the live model;
/// equal indices draw nothing. halfWidth and worldY are signed world-coordinate
/// units. Each end extends by half the X/Z span, making the quad twice as long
/// as the limb. Part positions, worldY and corners narrow to signed halfwords.
/// shade is grey texture modulation (0..255). A negative GTE FLAG discards the
/// quad; this is a projection-status test, not a complete screen clipping test.
///
/// Requires initialized scratch storage and room for a POLY_FT4 in the frame
/// arena and current depth table. Refreshes coordinate caches and changes GTE
/// state. Releases its scratch block before returning; a queued packet stays
/// live until frame DMA completes.
static void _madChaserDrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 halfWidth, s32 worldY, u8 shade)
{
    enum { MAD_CHASER_SHADOW_TRIG_FRACTION_BITS = 12 };
    MadChaserLimbShadowScratch* scratch;
    s16                         segmentYaw;
    GfxCoord*                   secondCoord;
    GfxCoord*                   firstCoord;
    s32                         widthCosine;
    GfxCoord*                   partCoords;

    partCoords  = task->extra.tmd->coords;
    firstCoord  = partCoords + firstJoint;
    secondCoord = partCoords + secondJoint;
    if (firstJoint != secondJoint) {
        // Remove the view transform to recover the joints in world space.
        scratch = SCRATCH_STACK_RESERVE_BLOCK(MadChaserLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &scratch->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &scratch->secondMatrix);
        scratch->firstPos.vy  = (s16)worldY;
        scratch->secondPos.vy = (s16)worldY;
        scratch->firstPos.vx  = scratch->firstMatrix.t[0];
        scratch->firstPos.vz  = scratch->firstMatrix.t[2];
        scratch->secondPos.vx = scratch->secondMatrix.t[0];
        scratch->secondPos.vz = scratch->secondMatrix.t[2];
        // Widen perpendicular to the limb and overhang each end by half its span.
        segmentYaw             = ratan2(scratch->secondPos.vx - scratch->firstPos.vx, scratch->secondPos.vz - scratch->firstPos.vz);
        scratch->halfSpanX     = (scratch->firstPos.vx - scratch->secondPos.vx) / 2;
        scratch->halfSpanZ     = (scratch->firstPos.vz - scratch->secondPos.vz) / 2;
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[0].vy = (s16)worldY;
        scratch->corners[0].vx = scratch->halfSpanX + (scratch->firstPos.vx - (widthCosine >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[0].vz = scratch->halfSpanZ + (scratch->firstPos.vz + ((rsin(segmentYaw) * halfWidth) >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS));
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[1].vy = (s16)worldY;
        scratch->corners[1].vx = scratch->halfSpanX + (scratch->firstPos.vx + (widthCosine >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[1].vz = scratch->halfSpanZ + (scratch->firstPos.vz - ((rsin(segmentYaw) * halfWidth) >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS));
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[2].vy = (s16)worldY;
        scratch->corners[2].vx = (scratch->secondPos.vx - (widthCosine >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS)) - scratch->halfSpanX;
        scratch->corners[2].vz = (scratch->secondPos.vz + ((rsin(segmentYaw) * halfWidth) >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS)) - scratch->halfSpanZ;
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[3].vy = (s16)worldY;
        scratch->corners[3].vx = (scratch->secondPos.vx + (widthCosine >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS)) - scratch->halfSpanX;
        scratch->corners[3].vz = (scratch->secondPos.vz - ((rsin(segmentYaw) * halfWidth) >> MAD_CHASER_SHADOW_TRIG_FRACTION_BITS)) - scratch->halfSpanZ;
        // Project world corners through the freshly composed view transform.
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        scratch->depth = RotTransPers4(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2], &scratch->corners[3], &scratch->screenCorners[0], &scratch->screenCorners[1],
                                       &scratch->screenCorners[2], &scratch->screenCorners[3], &scratch->depthCue, &scratch->flag);
        if (scratch->flag >= 0) {
            _madChaserQueueLimbShadow(scratch, shade);
        }
        SCRATCH_STACK_RELEASE_BLOCK(MadChaserLimbShadowScratch);
    }
}
