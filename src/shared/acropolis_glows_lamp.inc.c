/* Part of the Acropolis glows library; see acropolis_glows.h. */

/// Reserves a textured quad and initializes its DMA length and packet code.
///
/// Requires word-aligned space at `gGpuPrimCursor`; no capacity check occurs.
/// Only the packet header is initialized. The caller fills and links the quad;
/// its frame-arena storage must remain live until GPU completion.
static __inline__ POLY_FT4* _glowLampReserveQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

void GLOW_LAMP_TASK(Task* task)
{
    enum {
        GLOW_LAMP_VARIANT_COUNT               = 3,
        GLOW_LAMP_FIRST_TEXTURE_COLUMN        = 1,
        GLOW_LAMP_FIRST_PALETTE_OFFSET        = 2,
        GLOW_LAMP_TEXTURE_TOP_V               = 16,
        GLOW_LAMP_HALF_EXTENT_DEPTH_PRODUCT   = 0x6180, // Pixel half-side times camera Z / 4
        GLOW_LAMP_BRIGHT_BASE_INTENSITY       = 96,
        GLOW_LAMP_LAST_VARIANT_INTENSITY_STEP = 12
    };
    GfxCoord*              coord;
    EffectWork*            work;
    RoomGlowSpriteScratch* projection;
    POLY_FT4*              quad;
    s32                    intensity;
    s32                    paletteWord;

    // Project the composed origin after narrowing its world coordinates to s16.
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    projection->worldPos.vx = coord->workm.t[0];
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    quad = _glowLampReserveQuad();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        u8 baseIntensity[GLOW_LAMP_VARIANT_COUNT]         = { GLOW_FLICKER_BASE_INTENSITY, GLOW_LAMP_BRIGHT_BASE_INTENSITY, GLOW_FLICKER_BASE_INTENSITY };
        u8 oddFrameIntensityStep[GLOW_LAMP_VARIANT_COUNT] = { GLOW_FLICKER_INTENSITY_STEP, 1 << GLOW_BRIGHT_FLICKER_SHIFT, GLOW_LAMP_LAST_VARIANT_INTENSITY_STEP };

        // The full spawn word selects one of three independently tinted texture cells.
        intensity = baseIntensity[task->spawnArg1.value] + (gDisplayState.animFrame & 1) * oddFrameIntensityStep[task->spawnArg1.value];
        setSemiTrans(quad, 1);
        quad->tpage = GLOW_FLARE_TEXTURE_PAGE;
        quad->r0    = intensity;
        quad->g0    = intensity;
        quad->b0    = intensity;
        // Keep the palette expression word-sized before the halfword packet store.
        paletteWord            = ((task->spawnArg1.value + GLOW_LAMP_FIRST_PALETTE_OFFSET) & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        quad->clut             = paletteWord;
        quad->u0               = (task->spawnArg1.value + GLOW_LAMP_FIRST_TEXTURE_COLUMN) * GLOW_FLARE_CELL_STRIDE;
        quad->v0               = GLOW_LAMP_TEXTURE_TOP_V;
        quad->u1               = (task->spawnArg1.value + GLOW_LAMP_FIRST_TEXTURE_COLUMN) * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
        quad->v1               = GLOW_LAMP_TEXTURE_TOP_V;
        quad->u2               = (task->spawnArg1.value + GLOW_LAMP_FIRST_TEXTURE_COLUMN) * GLOW_FLARE_CELL_STRIDE;
        quad->v2               = GLOW_LAMP_TEXTURE_TOP_V + GLOW_FLARE_CELL_LAST_TEXEL;
        quad->u3               = (task->spawnArg1.value + GLOW_LAMP_FIRST_TEXTURE_COLUMN) * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
        quad->v3               = GLOW_LAMP_TEXTURE_TOP_V + GLOW_FLARE_CELL_LAST_TEXEL;
        projection->halfExtent = GLOW_LAMP_HALF_EXTENT_DEPTH_PRODUCT / projection->otz;
        quad->x0 = quad->x2 = projection->screenPos.vx - projection->halfExtent;
        quad->x1 = quad->x3 = projection->screenPos.vx + projection->halfExtent;
        quad->y0 = quad->y1 = projection->screenPos.vy - projection->halfExtent;
        quad->y2 = quad->y3 = projection->screenPos.vy + projection->halfExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    // A clipped draw still consumes its packet and retires this counted one-shot effect.
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(work, task);
}

#undef GLOW_LAMP_TASK
