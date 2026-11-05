/* Part of the glow drawing library; see glow.h. */

/// Draws two flickering grey light shafts from the carrier's `GLOW_TWIN_SHAFT_POINTS`, in `coord`'s space: both rooted
/// at points 14 and 17, their far ends four times as far out as points 15 and 18, and 16 and 19. A shaft whose far end
/// is too near is not drawn.
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

    /// Moves a shaft corner into world space through `coord` into `block`'s vertex `index`.
#define GLOW_TWIN_SHAFT_TRANSFORM_CORNER(point, index)         \
    (void)({                                                   \
        gte_SetRotMatrix(&coord->workm);                       \
        gte_ldv0((point));                                     \
        gte_rtv0();                                            \
        gte_stsv(&block->vertices[(index)]);                   \
        block->vertices[(index)].vx += (u32)coord->workm.t[0]; \
        block->vertices[(index)].vy += (u32)coord->workm.t[1]; \
        block->vertices[(index)].vz += (u32)coord->workm.t[2]; \
    })

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);

    // Transform the shared root edge once; both quads reuse it.
    gte_SetTransMatrix(&GsWSMATRIX);
    GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT], 0);

    GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT], 1);

    // Extend each root-to-guide offset fourfold before world projection.
    for (shaftIndex = 0; shaftIndex < GLOW_TWIN_SHAFT_COUNT; shaftIndex++) {
        pointIndex            = shaftIndex + GLOW_TWIN_SHAFT_FIRST_TIP;
        firstTipPoint         = &GLOW_TWIN_SHAFT_POINTS[pointIndex];
        block->vertices[2].vx = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vx +
                                ((u16)firstTipPoint->vx - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vx) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[2].vy = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vy +
                                ((u16)firstTipPoint->vy - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vy) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[2].vz = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vz +
                                ((u16)firstTipPoint->vz - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_FIRST_ROOT].vz) * GLOW_TWIN_SHAFT_REACH_SCALE;
        GLOW_TWIN_SHAFT_TRANSFORM_CORNER(&block->vertices[2], 2);

        pointIndex            = shaftIndex + GLOW_TWIN_SHAFT_SECOND_TIP;
        secondTipPoint        = &GLOW_TWIN_SHAFT_POINTS[pointIndex];
        block->vertices[3].vx = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vx +
                                ((u16)secondTipPoint->vx - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vx) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[3].vy = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vy +
                                ((u16)secondTipPoint->vy - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vy) * GLOW_TWIN_SHAFT_REACH_SCALE;
        block->vertices[3].vz = (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vz +
                                ((u16)secondTipPoint->vz - (u16)GLOW_TWIN_SHAFT_POINTS[GLOW_TWIN_SHAFT_SECOND_ROOT].vz) * GLOW_TWIN_SHAFT_REACH_SCALE;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    primitive);
            gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
#undef GLOW_TWIN_SHAFT_TRANSFORM_CORNER
}
