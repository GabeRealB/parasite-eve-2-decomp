/* Part of the muzzle flash library; see muzzle_flash.h. */

/// Sets the pixel offsets for one opposite-corner pair of the muzzle-flash core.
///
/// `spriteScratch->otz` must contain the projected centre's SZ3 / 4 depth,
/// at least `MUZZLE_FLASH_MIN_DEPTH`. `worldSize` is the nonnegative size
/// scale supplied by the flash task (0..2559). Integer division first gives
/// a pixel radius of worldSize * 55 / otz; Q12 sine/cosine then produce signed
/// offsets without narrowing them to the packet's 16-bit coordinates.
/// `cornerAngle` uses 4096 units per turn: zero points up, a quarter turn
/// right. The caller adds/subtracts `cornerDx` along X and subtracts/adds
/// `cornerDy` along screen Y, then repeats a quarter turn later for the other
/// pair. Only those two scratch words are written; no address is retained.
static inline void _muzzleFlashSetCoreCornerOffsets(OverlaySpriteScratch* spriteScratch, s16 worldSize, s32 cornerAngle)
{
    enum { MUZZLE_FLASH_CORE_PROJECTION_SCALE = 55 };
    spriteScratch->cornerDx = (((worldSize * MUZZLE_FLASH_CORE_PROJECTION_SCALE) / spriteScratch->otz) * rsin(cornerAngle)) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    spriteScratch->cornerDy = (((worldSize * MUZZLE_FLASH_CORE_PROJECTION_SCALE) / spriteScratch->otz) * rcos(cornerAngle)) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
}

/// Draws the additive textured core as a spinning camera-facing square.
///
/// `muzzleCoord->workm` must be freshly composed through the view chain; its
/// translation supplies the centre, narrowed to signed 16-bit coordinates.
/// `worldSize` is a nonnegative game-coordinate radius scale (the task starts
/// at 1536..2559); the screen corner radius is worldSize * 55 / (SZ3 / 4).
/// `spinAngle` uses 4096 units per turn. The raw texture supplies all colour.
/// Requires initialized projection settings, a free scratch block and space
/// for one POLY_FT4 packet. The packet is consumed even below the depth cutoff.
static void _muzzleFlashDrawCore(const GfxCoord* muzzleCoord, s16 worldSize, s16 spinAngle)
{
    enum {
        MUZZLE_FLASH_CORE_TEXTURE_LEFT     = 0x70,
        MUZZLE_FLASH_CORE_TEXTURE_TOP      = 0xC8,
        MUZZLE_FLASH_CORE_TEXTURE_RIGHT    = 0xA7,
        MUZZLE_FLASH_CORE_TEXTURE_BOTTOM   = 0xFF,
        MUZZLE_FLASH_CORE_SEMI_TRANSPARENT = 0x02,
        MUZZLE_FLASH_CORE_RAW_TEXTURE      = 0x01
    };
    OverlaySpriteScratch* block;
    POLY_FT4*             quad;
    s32                   cornerAngle;

    // Project the composed muzzle centre; the quad itself stays in screen space.
    block              = SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
    block->worldPos.vx = muzzleCoord->workm.t[0];
    block->worldPos.vy = muzzleCoord->workm.t[1];
    block->worldPos.vz = muzzleCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= MUZZLE_FLASH_MIN_DEPTH) {
        cornerAngle = spinAngle;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 0x240, 0);
        quad->clut  = getClut(0xB0, 0x10A);
        setUV4(quad, MUZZLE_FLASH_CORE_TEXTURE_LEFT, MUZZLE_FLASH_CORE_TEXTURE_TOP,
               MUZZLE_FLASH_CORE_TEXTURE_RIGHT, MUZZLE_FLASH_CORE_TEXTURE_TOP,
               MUZZLE_FLASH_CORE_TEXTURE_LEFT, MUZZLE_FLASH_CORE_TEXTURE_BOTTOM,
               MUZZLE_FLASH_CORE_TEXTURE_RIGHT, MUZZLE_FLASH_CORE_TEXTURE_BOTTOM);
        setcode(quad, getcode(quad) | MUZZLE_FLASH_CORE_SEMI_TRANSPARENT | MUZZLE_FLASH_CORE_RAW_TEXTURE);

        // Opposite corner pairs are a quarter turn apart around the projected centre.
        _muzzleFlashSetCoreCornerOffsets(block, worldSize, cornerAngle);
        quad->x0     = block->screenPos.vx + block->cornerDx;
        quad->x3     = block->screenPos.vx - block->cornerDx;
        quad->y0     = block->screenPos.vy - block->cornerDy;
        cornerAngle += MUZZLE_FLASH_ANGLE_QUARTER_TURN;
        quad->y3     = block->screenPos.vy + block->cornerDy;
        _muzzleFlashSetCoreCornerOffsets(block, worldSize, cornerAngle);
        quad->x1 = block->screenPos.vx + block->cornerDx;
        quad->x2 = block->screenPos.vx - block->cornerDx;
        quad->y1 = block->screenPos.vy - block->cornerDy;
        quad->y2 = block->screenPos.vy + block->cornerDy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
}
