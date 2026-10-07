/* Part of the limb shadows library; see limb_shadows.h. */

/// Queues a subtractive textured quad from an accepted limb-shadow projection.
///
/// Borrows the four packed screen-XY words and sorting depth in `scratch`.
/// The frame arena must have room for one word-aligned `POLY_FT4`, and the
/// current ordering table must provide its 1024 depth tags. The packet remains
/// live until the frame DMA finishes; the scratch block is not retained.
static inline void _limbShadowQueueSegment(const ActorLimbShadowScratch* scratch, u8 shade)
{
    // Texture format, texel bounds, and VRAM page/palette coordinates.
    enum {
        LIMB_SHADOW_TEXTURE_4_BIT  = 0,
        LIMB_SHADOW_TEXTURE_U_MIN  = 0xC0,
        LIMB_SHADOW_TEXTURE_U_MAX  = 0xF7,
        LIMB_SHADOW_TEXTURE_V_MIN  = 0x98,
        LIMB_SHADOW_TEXTURE_V_MAX  = 0xCF,
        LIMB_SHADOW_TEXTURE_PAGE_X = 512,
        LIMB_SHADOW_TEXTURE_PAGE_Y = 0,
        LIMB_SHADOW_TEXTURE_CLUT_X = 48,
        LIMB_SHADOW_TEXTURE_CLUT_Y = 266,
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
    setUV4(quad, LIMB_SHADOW_TEXTURE_U_MIN, LIMB_SHADOW_TEXTURE_V_MIN, LIMB_SHADOW_TEXTURE_U_MAX, LIMB_SHADOW_TEXTURE_V_MIN,
           LIMB_SHADOW_TEXTURE_U_MIN, LIMB_SHADOW_TEXTURE_V_MAX, LIMB_SHADOW_TEXTURE_U_MAX, LIMB_SHADOW_TEXTURE_V_MAX);
    quad->tpage = getTPage(LIMB_SHADOW_TEXTURE_4_BIT, GPU_BLEND_SUBTRACT, LIMB_SHADOW_TEXTURE_PAGE_X, LIMB_SHADOW_TEXTURE_PAGE_Y);
    quad->clut  = getClut(LIMB_SHADOW_TEXTURE_CLUT_X, LIMB_SHADOW_TEXTURE_CLUT_Y);
    setRGB0(quad, shade, shade, shade);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), quad);
}

/// Queues a horizontal limb-shadow quad between two model parts.
///
/// `firstJoint` and `secondJoint` must index the live actor model's coordinates
/// in 0..partCount-1. Equal indices draw nothing. `halfWidth` and the plane's
/// `worldY` are signed world units; each end overhangs by half the X/Z segment,
/// making the quad twice as long. Part translations and corners narrow to s16.
/// `shade` is grey texture modulation (0..255) with subtractive blending.
/// A negative GTE FLAG discards the quad, without reserving a GPU packet.
///
/// Requires initialized scratch storage, room in the frame arena for one
/// `POLY_FT4`, and the current depth ordering table. Refreshes coordinate caches
/// and changes GTE state. Releases its scratch block before returning; the
/// queued packet remains live until the frame DMA finishes.
static void _limbShadowDrawSegment(Task* actor, s16 firstJoint, s16 secondJoint, s16 halfWidth, s16 worldY, u8 shade)
{
    // SDK sine/cosine values have 12 fractional bits: 4096 represents 1.0.
    enum { LIMB_SHADOW_TRIG_FRACTION_BITS = 12 };
    ActorLimbShadowScratch* scratch;
    s16                     segmentYaw; // 4096 units per turn
    GfxCoord*               secondCoord;
    GfxCoord*               firstCoord;
    s32                     widthCosine;
    s32                     halfSpanX;
    s32                     halfSpanZ;
    GfxCoord*               partCoords;

    partCoords  = actor->extra.tmd->coords;
    firstCoord  = partCoords + firstJoint;
    secondCoord = partCoords + secondJoint;
    if (firstJoint != secondJoint) {
        // Remove the view transform to recover both parts' world positions.
        scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &scratch->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &scratch->secondMatrix);
        scratch->firstPos.vy  = worldY;
        scratch->secondPos.vy = worldY;
        scratch->firstPos.vx  = scratch->firstMatrix.t[0];
        scratch->firstPos.vz  = scratch->firstMatrix.t[2];
        scratch->secondPos.vx = scratch->secondMatrix.t[0];
        scratch->secondPos.vz = scratch->secondMatrix.t[2];
        // Widen perpendicular to the segment and extend both ends by half its span.
        segmentYaw             = ratan2(scratch->secondPos.vx - scratch->firstPos.vx, scratch->secondPos.vz - scratch->firstPos.vz);
        halfSpanX              = (scratch->firstPos.vx - scratch->secondPos.vx) / 2;
        halfSpanZ              = (scratch->firstPos.vz - scratch->secondPos.vz) / 2;
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[0].vy = worldY;
        scratch->corners[0].vx = halfSpanX + (scratch->firstPos.vx - (widthCosine >> LIMB_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[0].vz = halfSpanZ + (scratch->firstPos.vz + ((rsin(segmentYaw) * halfWidth) >> LIMB_SHADOW_TRIG_FRACTION_BITS));
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[1].vy = worldY;
        scratch->corners[1].vx = halfSpanX + (scratch->firstPos.vx + (widthCosine >> LIMB_SHADOW_TRIG_FRACTION_BITS));
        scratch->corners[1].vz = halfSpanZ + (scratch->firstPos.vz - ((rsin(segmentYaw) * halfWidth) >> LIMB_SHADOW_TRIG_FRACTION_BITS));
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[2].vy = worldY;
        scratch->corners[2].vx = (scratch->secondPos.vx - (widthCosine >> LIMB_SHADOW_TRIG_FRACTION_BITS)) - halfSpanX;
        scratch->corners[2].vz = (scratch->secondPos.vz + ((rsin(segmentYaw) * halfWidth) >> LIMB_SHADOW_TRIG_FRACTION_BITS)) - halfSpanZ;
        widthCosine            = rcos(segmentYaw) * halfWidth;
        scratch->corners[3].vy = worldY;
        scratch->corners[3].vx = (scratch->secondPos.vx + (widthCosine >> LIMB_SHADOW_TRIG_FRACTION_BITS)) - halfSpanX;
        scratch->corners[3].vz = (scratch->secondPos.vz - ((rsin(segmentYaw) * halfWidth) >> LIMB_SHADOW_TRIG_FRACTION_BITS)) - halfSpanZ;
        // Project the world corners through a freshly composed view transform.
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        scratch->depth = RotTransPers4(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2], &scratch->corners[3], &scratch->screenCorners[0], &scratch->screenCorners[1],
                                       &scratch->screenCorners[2], &scratch->screenCorners[3], &scratch->depthCue, &scratch->flag);
        if (scratch->flag >= 0) {
            _limbShadowQueueSegment(scratch, shade);
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorLimbShadowScratch);
    }
}
