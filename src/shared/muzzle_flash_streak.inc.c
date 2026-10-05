/* Part of the muzzle flash library; see muzzle_flash.h. */

/// Draws one additive blue-white streak in the muzzle's coordinate frame.
///
/// `muzzleCoord->workm` must be freshly composed through the view chain.
/// Angles use 4096 units per turn. Three base corners have radius 256 around
/// `directionAngle`, at offsets -192, 0 and +192; the tip has radius 1536 and
/// local Z -512. The muzzle transform rotates that Z offset with the weapon.
/// `brightness` is 0..255; only the centre base corner is lit, with half
/// brightness in red/green and full brightness in blue. Positions narrow to
/// signed 16-bit coordinates before projection. Requires initialized projection
/// settings, one free scratch block and packet space for POLY_G4 plus its blend
/// command. A rejected depth still consumes the quad packet.
static void _muzzleFlashDrawStreak(const GfxCoord* muzzleCoord, s16 directionAngle, s16 brightness)
{
    enum {
        MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET = 0xC0,
        MUZZLE_FLASH_STREAK_TIP_RADIUS        = 0x600,
        MUZZLE_FLASH_STREAK_TIP_LOCAL_Z       = -0x200,
        MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT   = 4 // 4096-amplitude trig to a radius of 256
    };
    EffectQuadCornersScratch* block;
    POLY_G4*                  quad;
    const MATRIX*             muzzleMatrix;
    s32                       cornerAngle;
    s32                       firstBaseAngle;
    s32                       tipRadius;
    s32                       tipLocalZ;

    /// Rotates one initialized local corner in place through the composed muzzle matrix.
    ///
    /// Both pointer arguments must be side-effect-free: corner is evaluated twice,
    /// matrix once. Captures no C locals and clobbers GTE working registers.
    /// Expands to four statements; use only as a standalone statement sequence.
#define MUZZLE_FLASH_ROTATE_STREAK_CORNER(corner, matrix) \
    gte_SetRotMatrix(matrix);                             \
    gte_ldv0(corner);                                     \
    gte_rtv0();                                           \
    gte_stsv(corner)

    // Keep the tip dimensions in locals at their first use: the target uses mult.
    tipLocalZ = MUZZLE_FLASH_STREAK_TIP_LOCAL_Z;
    block     = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerAngle = directionAngle;

    // Build local corners, then rotate and add translations modulo 16 bits.
    // Unsigned addition preserves wrapping even for a full-width translation.
    firstBaseAngle        = cornerAngle - MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET;
    block->vertices[0].vx = (u32)rsin(firstBaseAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[0].vy = (u32)rcos(firstBaseAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[0].vz = 0;
    muzzleMatrix          = &muzzleCoord->workm;
    MUZZLE_FLASH_ROTATE_STREAK_CORNER(&block->vertices[0], muzzleMatrix);
    block->vertices[0].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[0].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[0].vz += (u32)muzzleCoord->workm.t[2];

    tipRadius             = MUZZLE_FLASH_STREAK_TIP_RADIUS;
    block->vertices[1].vx = (rsin(cornerAngle) * tipRadius) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    block->vertices[1].vy = (rcos(cornerAngle) * tipRadius) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    block->vertices[1].vz = tipLocalZ;
    MUZZLE_FLASH_ROTATE_STREAK_CORNER(&block->vertices[1], muzzleMatrix);
    block->vertices[1].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[1].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[1].vz += (u32)muzzleCoord->workm.t[2];

    block->vertices[2].vx = (u32)rsin(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[2].vy = (u32)rcos(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[2].vz = 0;
    MUZZLE_FLASH_ROTATE_STREAK_CORNER(&block->vertices[2], muzzleMatrix);
    block->vertices[2].vx += (u32)muzzleCoord->workm.t[0];
    cornerAngle            = cornerAngle + MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET;
    block->vertices[2].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[2].vz += (u32)muzzleCoord->workm.t[2];

    block->vertices[3].vx = (u32)rsin(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[3].vy = (u32)rcos(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[3].vz = 0;
    MUZZLE_FLASH_ROTATE_STREAK_CORNER(&block->vertices[3], muzzleMatrix);
    block->vertices[3].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[3].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[3].vz += (u32)muzzleCoord->workm.t[2];

    // Project corner 0 separately, then corners 1..3 into the packet.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vertices[0]);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&block->vertices[1], &block->vertices[2],
             &block->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&block->depth);
    if (block->depth >= MUZZLE_FLASH_MIN_DEPTH) {
        setRGB0(quad, 0, 0, 0);
        setRGB1(quad, 0, 0, 0);
        setRGB2(quad, brightness >> 1, brightness >> 1, brightness);
        setRGB3(quad, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
#undef MUZZLE_FLASH_ROTATE_STREAK_CORNER
}
