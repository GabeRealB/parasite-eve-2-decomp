/* Part of the Acropolis glows library; see acropolis_glows.h. */

// Select the core quad's half-height storage for this included instance.
#if defined(GLOW_STAR_STORE_HALF_HEIGHT) && GLOW_STAR_STORE_HALF_HEIGHT
#define ACROPOLIS_GLOWS_STAR_HALF_HEIGHT block->cornerDy
#else
#define ACROPOLIS_GLOWS_STAR_HALF_HEIGHT block->cornerDx
#endif

/// Reserves and initializes one textured quad in the current frame's packet arena.
///
/// `quad` must be a writable `POLY_FT4*` lvalue without side effects; it is
/// evaluated four times. Captures and advances `gGpuPrimCursor`, which must have
/// aligned space for the packet. The caller fills and links it; the arena must
/// remain live until GPU completion. Requires the SDK's `setPolyFT4` macro.
/// The comma expression returns the assigned packet-code byte; use it as a statement.
#define GLOW_STAR_RESERVE_QUAD(quad) \
    ((quad) = gGpuPrimCursor, gGpuPrimCursor = (quad) + 1, setPolyFT4(quad))

void GLOW_STAR_TASK(Task* task)
{
    enum {
        GLOW_STAR_CORE_FRAME_COUNT               = 6,
        GLOW_STAR_CORE_CELL_STRIDE               = 16,     // Texels; each core cell is 16 by 16
        GLOW_STAR_FLARE_TOP_V                    = 16,
        GLOW_STAR_CORE_HALF_EXTENT_DEPTH_PRODUCT = 0x1680, // Pixel half-side times projected depth
        GLOW_STAR_FLARE_RADIUS_DEPTH_PRODUCT     = 0x3A80, // Pixel corner radius times projected depth
        GLOW_STAR_FLARE_GREY_LEVEL_COUNT         = 96,
        GLOW_STAR_SEMITRANSPARENT                = 2,
        GLOW_STAR_UNMODULATED_TEXTURE            = 1
    };
    GfxCoord*             coord;
    EffectWork*           work;
    OverlaySpriteScratch* previousTop;
    OverlaySpriteScratch* block;
    s32*                  depthOutput;
    POLY_FT4*             quad;
    s32                   flareIntensity;

    // Project the composed centre, narrowing its world coordinates to signed halfwords.
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    work->age   = task->spawnArg1.value;
    previousTop = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    block       = previousTop - 1;
    depthOutput = &block->otz;

    block->worldPos.vx = coord->workm.t[0];
    block->worldPos.vy = coord->workm.t[1];

    SCRATCH_STACK_CURSOR(OverlaySpriteScratch) = block;

    block->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    GLOW_STAR_RESERVE_QUAD(quad);
    gte_stsxy(&block->screenPos);
    gte_stszotz(depthOutput);
    if (block->otz >= GLOW_MIN_DEPTH) {
        // The upright core uses the signed spawn phase to select its texture cell.
        quad->tpage     = GLOW_FLARE_TEXTURE_PAGE;
        quad->clut      = GLOW_FLARE_PALETTE_BASE;
        quad->code     |= GLOW_STAR_SEMITRANSPARENT | GLOW_STAR_UNMODULATED_TEXTURE;
        quad->u0        = (work->age % GLOW_STAR_CORE_FRAME_COUNT) * GLOW_STAR_CORE_CELL_STRIDE;
        quad->v0        = 0;
        quad->u1        = (work->age % GLOW_STAR_CORE_FRAME_COUNT) * GLOW_STAR_CORE_CELL_STRIDE + GLOW_STAR_CORE_CELL_STRIDE - 1;
        quad->v1        = 0;
        quad->u2        = (work->age % GLOW_STAR_CORE_FRAME_COUNT) * GLOW_STAR_CORE_CELL_STRIDE;
        quad->v2        = GLOW_STAR_CORE_CELL_STRIDE - 1;
        quad->u3        = (work->age % GLOW_STAR_CORE_FRAME_COUNT) * GLOW_STAR_CORE_CELL_STRIDE + GLOW_STAR_CORE_CELL_STRIDE - 1;
        quad->v3        = GLOW_STAR_CORE_CELL_STRIDE - 1;
        block->cornerDx = GLOW_STAR_CORE_HALF_EXTENT_DEPTH_PRODUCT / block->otz;
#if defined(GLOW_STAR_STORE_HALF_HEIGHT) && GLOW_STAR_STORE_HALF_HEIGHT
        block->cornerDy = GLOW_STAR_CORE_HALF_EXTENT_DEPTH_PRODUCT / block->otz;
#endif
        quad->x0 = quad->x2 = block->screenPos.vx - block->cornerDx;
        quad->x1 = quad->x3 = block->screenPos.vx + block->cornerDx;
        quad->y0 = quad->y1 = block->screenPos.vy - ACROPOLIS_GLOWS_STAR_HALF_HEIGHT;
        quad->y2 = quad->y3 = block->screenPos.vy + ACROPOLIS_GLOWS_STAR_HALF_HEIGHT;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);

        // Tint and rotate the larger flare in Q12, keeping the halfword phase wrap.
        GLOW_STAR_RESERVE_QUAD(quad);
        quad->clut      = GLOW_FLARE_PALETTE_BASE + 1;
        quad->tpage     = GLOW_FLARE_TEXTURE_PAGE;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        flareIntensity  = (gRandomLcgState >> 16) % GLOW_STAR_FLARE_GREY_LEVEL_COUNT + GLOW_FLICKER_BASE_INTENSITY;
        quad->u0        = 0;
        quad->v0        = GLOW_STAR_FLARE_TOP_V;
        quad->u1        = GLOW_FLARE_CELL_LAST_TEXEL;
        quad->v1        = GLOW_STAR_FLARE_TOP_V;
        quad->u2        = 0;
        quad->v2        = GLOW_STAR_FLARE_TOP_V + GLOW_FLARE_CELL_LAST_TEXEL;
        quad->u3        = GLOW_FLARE_CELL_LAST_TEXEL;
        quad->v3        = GLOW_STAR_FLARE_TOP_V + GLOW_FLARE_CELL_LAST_TEXEL;
        quad->code     |= GLOW_STAR_SEMITRANSPARENT;
        quad->r0        = flareIntensity;
        quad->g0        = flareIntensity;
        quad->b0        = flareIntensity;

        work->scale     = gDisplayState.animFrame + work->age;
        block->cornerDx = ((GLOW_STAR_FLARE_RADIUS_DEPTH_PRODUCT / block->otz) * rsin(work->scale)) >> GLOW_TRIG_SHIFT;
        block->cornerDy = ((GLOW_STAR_FLARE_RADIUS_DEPTH_PRODUCT / block->otz) * rcos(work->scale)) >> GLOW_TRIG_SHIFT;
        quad->x0        = block->screenPos.vx + block->cornerDx;
        quad->x3        = block->screenPos.vx - block->cornerDx;
        quad->y0        = block->screenPos.vy - block->cornerDy;
        quad->y3        = block->screenPos.vy + block->cornerDy;
        block->cornerDx = ((GLOW_STAR_FLARE_RADIUS_DEPTH_PRODUCT / block->otz) * rsin(work->scale + GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT;
        block->cornerDy = ((GLOW_STAR_FLARE_RADIUS_DEPTH_PRODUCT / block->otz) * rcos(work->scale + GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT;
        quad->x1        = block->screenPos.vx + block->cornerDx;
        quad->x2        = block->screenPos.vx - block->cornerDx;
        quad->y1        = block->screenPos.vy - block->cornerDy;
        quad->y2        = block->screenPos.vy + block->cornerDy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    // A clipped core still consumes its packet; every path releases this one-shot effect.
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
    effectKillTask(work, task);
}

#undef GLOW_STAR_TASK
#undef GLOW_STAR_RESERVE_QUAD
#undef GLOW_STAR_STORE_HALF_HEIGHT
#undef ACROPOLIS_GLOWS_STAR_HALF_HEIGHT
