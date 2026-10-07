/* Part of the Diver library; see diver.h. */

/// Computes one perspective-scaled billboard corner offset in screen pixels.
///
/// `size` is the unsigned perspective scale and `cornerAngle` uses 4096 units
/// per turn, zero upward. Requires positive `block->depth`. Truncates the
/// half-diagonal before multiplying by Q12 sine and cosine; changes only the
/// two offset words. Products use signed word arithmetic and must fit.
static __inline__ void _diverComputeSparkCornerOffset(EffectBillboardScratch* block, u16 size, s32 cornerAngle)
{
    enum { DIVER_SPARK_UV_SPAN            = 39,
           DIVER_SPARK_TRIG_FRACTION_BITS = 12 };

    block->cornerOffsetX = (((size * DIVER_SPARK_UV_SPAN) / block->depth) * rsin(cornerAngle)) >> DIVER_SPARK_TRIG_FRACTION_BITS;
    block->cornerOffsetY = (((size * DIVER_SPARK_UV_SPAN) / block->depth) * rcos(cornerAngle)) >> DIVER_SPARK_TRIG_FRACTION_BITS;
}

/// Queues one additive, unmodulated impact-spark billboard.
///
/// Reads the composed translation of `coord` in the input space of `GsWSMATRIX`,
/// narrowing each component to signed 16 bits; it does not compose the coordinate.
/// `frameIndex` wraps modulo six over 40-by-40 texel cells. `size` is an unsigned
/// perspective scale: the screen half-diagonal is size * 39 / (SZ3 / 4 + 1).
/// `angle` narrows to s16, in 4096 units per turn; zero puts a corner upward.
/// Rejects negative GTE projection flags and preserves the component narrowing
/// before screen-coordinate additions. Signed sizing products must fit s32.
///
/// Requires the loaded texture/palette, initialized scratch stack and room for
/// one POLY_FT4 in the unchecked frame arena. Releases scratch before return;
/// the queued packet stays live through GPU drawing. Changes GTE state and
/// retains no input pointers.
static void _diverDrawSpark(const GfxCoord* coord, u16 frameIndex, u16 size, s32 angle)
{
    enum {
        DIVER_SPARK_CELL_SIZE    = 40,
        DIVER_SPARK_UV_SPAN      = DIVER_SPARK_CELL_SIZE - 1,
        DIVER_SPARK_TOP_V        = 56,
        DIVER_SPARK_BOTTOM_V     = DIVER_SPARK_TOP_V + DIVER_SPARK_UV_SPAN,
        DIVER_SPARK_QUARTER_TURN = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        DIVER_SPARK_QUAD_CODE    = 0x2F, // textured quad, semitransparent, raw texture
        DIVER_SPARK_TEXTURE_PAGE = getTPage(0, GPU_BLEND_ADD, 640, 0),
        DIVER_SPARK_CLUT         = getClut(304, 266)
    };
    void**                  scratch;
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    u16                     cellIndex;
    s32                     leftU;

    scratch                        = SCRATCH_HEAD_ADDR;
    scratchHead                    = SCRATCH_HEAD_AT(scratch, EffectBillboardScratch);
    block                          = scratchHead - 1;
    depthOutput                    = &block->depth;
    block->worldPoint.vx           = (u16)coord->workm.t[0];
    block->worldPoint.vy           = (u16)coord->workm.t[1];
    block->worldPoint.vz           = (u16)coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        quad->code  = DIVER_SPARK_QUAD_CODE;
        quad->tpage = DIVER_SPARK_TEXTURE_PAGE;
        quad->clut  = DIVER_SPARK_CLUT;
        cellIndex   = frameIndex % DIVER_SPARK_FRAME_COUNT;
        leftU       = cellIndex * DIVER_SPARK_CELL_SIZE;
        setUV4(quad, leftU, DIVER_SPARK_TOP_V, leftU + DIVER_SPARK_UV_SPAN, DIVER_SPARK_TOP_V,
               leftU, DIVER_SPARK_BOTTOM_V, leftU + DIVER_SPARK_UV_SPAN, DIVER_SPARK_BOTTOM_V);
        // The opposite corners share an offset; the other pair is a quarter turn later.
        cornerAngle = (s16)angle;
        _diverComputeSparkCornerOffset(block, size, cornerAngle);
        quad->x0    = block->screenX + (u16)block->cornerOffsetX;
        quad->x3    = block->screenX - (u16)block->cornerOffsetX;
        quad->y0    = block->screenY - (u16)block->cornerOffsetY;
        quad->y3    = block->screenY + (u16)block->cornerOffsetY;
        cornerAngle = cornerAngle + DIVER_SPARK_QUARTER_TURN;
        _diverComputeSparkCornerOffset(block, size, cornerAngle);
        quad->x1 = block->screenX + (u16)block->cornerOffsetX;
        quad->x2 = block->screenX - (u16)block->cornerOffsetX;
        quad->y1 = block->screenY - (u16)block->cornerOffsetY;
        quad->y2 = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(*block));
}
