/* Part of the glow drawing library; see glow_draw.h. */

/// Draws two additive flickering light shafts from the saloon's local point table.
///
/// Borrows an already composed local-to-world `coord`; it does not refresh it.
/// The carrier's `gSaloonLightPoints` must contain entries 14..19 in that
/// coordinate's local space. Both quads share roots 14 and 17; their tip
/// guides are 15/18 and 16/19. Each tip lies four times the root-to-guide
/// offset from its root. Local tip construction and transformed world
/// positions narrow to signed 16 bits before projection through `GsWSMATRIX`.
///
/// Root intensity alternates between 32 and 48 by animation-frame parity;
/// tips are black. Reserves two Gouraud packets even if clipped, and queues
/// each quad and its additive blend command only when the last tip's camera
/// Z / 4 is at least 17. Projection flags do not gate drawing. Packets belong
/// to the current frame arena; scratch corners are released before return.
static void _glowDrawTwinShafts(const GfxCoord* coord)
{
    enum {
        GLOW_TWIN_SHAFT_COUNT       = 2,
        GLOW_TWIN_SHAFT_FIRST_ROOT  = 14,
        GLOW_TWIN_SHAFT_SECOND_ROOT = 17,
        GLOW_TWIN_SHAFT_FIRST_TIP   = 15,
        GLOW_TWIN_SHAFT_SECOND_TIP  = 18,
        GLOW_TWIN_SHAFT_REACH_SCALE = 4,
    };

    EffectQuadCornersScratch* block;
    POLY_G4*                  primitive;
    const SVECTOR*            firstTipPoint;
    const SVECTOR*            secondTipPoint;
    s32                       shaftIndex;
    s32                       pointIndex;
    s32                       intensity;

    /// Transforms one shaft corner into a signed-halfword world position.
    ///
    /// Captures the composed coord and live scratch block. point is evaluated
    /// once and may alias the output corner; index is used four times and
    /// must be a pure value 0..3. Expands to a statement sequence without a
    /// scope, so invoke it only as a standalone sequence in a braced block.
    /// Unsigned additions preserve wrapping for full-width translations.
#define GLOW_TWIN_SHAFT_TRANSFORM_CORNER(point, index)     \
    gte_SetRotMatrix(&coord->workm);                       \
    gte_ldv0((point));                                     \
    gte_rtv0();                                            \
    gte_stsv(&block->vertices[(index)]);                   \
    block->vertices[(index)].vx += (u32)coord->workm.t[0]; \
    block->vertices[(index)].vy += (u32)coord->workm.t[1]; \
    block->vertices[(index)].vz += (u32)coord->workm.t[2]

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);

    // Transform the shared root edge once; both quads reuse it.
    gte_SetTransMatrix(&GsWSMATRIX);
    GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT], 0);

    GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT], 1);

    // Extend each root-to-guide offset fourfold before world projection.
    for (shaftIndex = 0; shaftIndex < GLOW_TWIN_SHAFT_COUNT; shaftIndex++) {
        pointIndex            = shaftIndex + GLOW_TWIN_SHAFT_FIRST_TIP;
        firstTipPoint         = &gSaloonLightPoints[pointIndex];
        block->vertices[2].vx = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vx +
                                ((u16)firstTipPoint->vx - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vx) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[2].vy = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vy +
                                ((u16)firstTipPoint->vy - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vy) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[2].vz = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vz +
                                ((u16)firstTipPoint->vz - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_FIRST_ROOT].vz) * GLOW_TWIN_SHAFT_REACH_SCALE;
        GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&block->vertices[2], 2);

        pointIndex            = shaftIndex + GLOW_TWIN_SHAFT_SECOND_TIP;
        secondTipPoint        = &gSaloonLightPoints[pointIndex];
        block->vertices[3].vx = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vx +
                                ((u16)secondTipPoint->vx - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vx) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[3].vy = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vy +
                                ((u16)secondTipPoint->vy - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vy) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[3].vz = (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vz +
                                ((u16)secondTipPoint->vz - (u16)gSaloonLightPoints[GLOW_TWIN_SHAFT_SECOND_ROOT].vz) * GLOW_TWIN_SHAFT_REACH_SCALE;
        GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&block->vertices[3], 3);

        // Sort and depth-clip by the final tip, retaining each allocated packet.
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vertices[0]);
        gte_rtps();
        primitive      = gGpuPrimCursor;
        gGpuPrimCursor = primitive + 1;
        setPolyG4(primitive);
        gte_stsxy(&primitive->x0);
        gte_ldv3(&block->vertices[1], &block->vertices[2],
                 &block->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&primitive->x1, &primitive->x2, &primitive->x3);
        gte_stszotz(&block->depth);
        if (block->depth >= GLOW_MIN_DEPTH) {
            intensity = ((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT) + GLOW_FLICKER_BASE_INTENSITY;
            setRGB2(primitive, 0, 0, 0);
            setRGB3(primitive, 0, 0, 0);
            setRGB0(primitive, intensity, intensity, intensity);
            setRGB1(primitive, intensity, intensity, intensity);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
#undef GLOW_TWIN_SHAFT_TRANSFORM_CORNER
}
