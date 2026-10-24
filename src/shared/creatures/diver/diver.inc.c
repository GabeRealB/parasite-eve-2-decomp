#include "main/random.h"

/* Part of the Diver library; see diver.h. */

/// Draws and spawns the launch, trail or impact recipe of a Diver strike.
///
/// `kind` is a DIVER_BURST_*; unlisted values spawn nothing. `phase` is the
/// caller's wrapping frame counter: two phases per billboard cell, room trail
/// particles on even phases and offset flashes every eighth phase.
/// `sizeAndSprayBias` bits 0..11 give billboard/flash size, bits 12..15 an
/// additive size bias for room particles; bits 16..31 are ignored. The bias
/// is added as an integer, rather than forwarded as a packed variant.
/// Room-particle addition can carry above bit 11 if size plus bias exceeds 4095.
/// Launch spawns one room particle; impact draws the billboard, spawns an upright
/// particle, then alternates four origin particles with four random-offset flashes.
///
/// Non-running control first draws a fixed-size spark. Cancellation then returns;
/// lower control values continue the selected recipe, including its spawns.
/// Requires live effect state, loaded room and flash task resources and a
/// composed strike coordinate suitable for the billboard. Spawn placement can
/// compose it again. Changes GTE and random state; spawn failures are ignored.
/// Flash placement copies the temporary offset during the call; its task never
/// follows the retained offset pointer after this function returns.
static void _diverImpactBurst(GfxCoord* coord, u16 phase, u16 kind, u32 sizeAndSprayBias)
{
    enum {
        DIVER_BURST_SIZE_MASK                 = 0xFFF,
        DIVER_BURST_SPRAY_BIAS_SHIFT          = 12,
        DIVER_BURST_SPRAY_BIAS_MASK           = 0xF,
        DIVER_BURST_PHASES_PER_CELL_SHIFT     = 1,
        DIVER_BURST_TRAIL_PARTICLE_PHASE_MASK = 1,
        DIVER_BURST_TRAIL_FLASH_PHASE_MASK    = 7,
        DIVER_BURST_FALLBACK_SPARK_SIZE       = 0x400,
        DIVER_BURST_FLASH_COUNT               = 4,
        DIVER_FLASH_OFFSET_RADIUS             = 64,
        // Room spray spawn words: upright/direction kind and one update per cell.
        DIVER_BURST_LAUNCH_PARTICLE         = 0x14001000,
        DIVER_BURST_TRAIL_PARTICLE          = 0x01000000,
        DIVER_BURST_IMPACT_UPRIGHT_PARTICLE = 0x10001000,
        DIVER_BURST_IMPACT_SPRAY_PARTICLE   = 0x02001000
    };
    SVECTOR flashOffset;
    s32     flashIndex;
    u16     spraySizeBias;
    u16     size;

/// Chooses a random strike-local flash offset at the requested integer radius.
///
/// `vector` is a writable SVECTOR lvalue and `vectorPtr` a writable SVECTOR*
/// local; both must be side-effect-free and occur repeatedly. Assigns XYZ
/// directly before taking the address, leaves pad untouched, advances the
/// shared LCG three times and normalizes/scales on the GTE. `radius` is an
/// integer strike-local distance, evaluated once. A zero direction is passed
/// to the SDK without a guard. Clobbers GTE state and leaves vectorPtr at vector.
/// Expands to a compound statement; invoke within a braced control-flow body.
#define DIVER_RANDOMIZE_FLASH_OFFSET(vector, vectorPtr, radius)                                                                        \
    {                                                                                                                                  \
        enum { DIVER_FLASH_RANDOM_CENTER     = 0x80,                                                                                   \
               DIVER_FLASH_RANDOM_BYTE_MASK  = 0xFF,                                                                                   \
               DIVER_FLASH_RANDOM_BYTE_SHIFT = 16 };                                                                                   \
        RANDOM_LCG_ADVANCE();                                                                                                          \
        (vector).vx = DIVER_FLASH_RANDOM_CENTER - ((gRandomLcgState >> DIVER_FLASH_RANDOM_BYTE_SHIFT) & DIVER_FLASH_RANDOM_BYTE_MASK); \
        RANDOM_LCG_ADVANCE();                                                                                                          \
        (vector).vy = DIVER_FLASH_RANDOM_CENTER - ((gRandomLcgState >> DIVER_FLASH_RANDOM_BYTE_SHIFT) & DIVER_FLASH_RANDOM_BYTE_MASK); \
        RANDOM_LCG_ADVANCE();                                                                                                          \
        (vector).vz = DIVER_FLASH_RANDOM_CENTER - ((gRandomLcgState >> DIVER_FLASH_RANDOM_BYTE_SHIFT) & DIVER_FLASH_RANDOM_BYTE_MASK); \
        (vectorPtr) = &(vector);                                                                                                       \
        VectorNormalSS((vectorPtr), (vectorPtr));                                                                                      \
        gte_lddp(radius);                                                                                                              \
        gte_ldsv(vectorPtr);                                                                                                           \
        gte_gpf12();                                                                                                                   \
        gte_stsv(vectorPtr);                                                                                                           \
    }

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _diverDrawSpark(coord, ((u32)phase >> DIVER_BURST_PHASES_PER_CELL_SHIFT) % DIVER_SPARK_FRAME_COUNT, DIVER_BURST_FALLBACK_SPARK_SIZE, 0);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    }

    // Decode sizes independently; the room-particle bias is never a variant selector.
    spraySizeBias = (sizeAndSprayBias >> DIVER_BURST_SPRAY_BIAS_SHIFT) & DIVER_BURST_SPRAY_BIAS_MASK;
    size          = sizeAndSprayBias & DIVER_BURST_SIZE_MASK;

    switch (kind) {
        case DIVER_BURST_LAUNCH:
            effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_LAUNCH_PARTICLE + size + spraySizeBias, NULL);
            break;

        case DIVER_BURST_TRAIL:
            _diverDrawSpark(coord, ((u32)phase >> DIVER_BURST_PHASES_PER_CELL_SHIFT) % DIVER_SPARK_FRAME_COUNT, size, 0);
            if (!(phase & DIVER_BURST_TRAIL_PARTICLE_PHASE_MASK)) {
                effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_TRAIL_PARTICLE + size + spraySizeBias, NULL);
            }
            if (!(phase & DIVER_BURST_TRAIL_FLASH_PHASE_MASK)) {
                SVECTOR* offsetPtr;

                DIVER_RANDOMIZE_FLASH_OFFSET(flashOffset, offsetPtr, DIVER_FLASH_OFFSET_RADIUS);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)size, offsetPtr);
            }
            break;

        case DIVER_BURST_IMPACT:
            _diverDrawSpark(coord, ((u32)phase >> DIVER_BURST_PHASES_PER_CELL_SHIFT) % DIVER_SPARK_FRAME_COUNT, size, 0);
            effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_IMPACT_UPRIGHT_PARTICLE + size + spraySizeBias, NULL);
            for (flashIndex = 0; flashIndex < DIVER_BURST_FLASH_COUNT; flashIndex++) {
                SVECTOR* offsetPtr;

                effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_IMPACT_SPRAY_PARTICLE + size + spraySizeBias, NULL);
                DIVER_RANDOMIZE_FLASH_OFFSET(flashOffset, offsetPtr, DIVER_FLASH_OFFSET_RADIUS);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)size, offsetPtr);
            }
            break;
    }
#undef DIVER_RANDOMIZE_FLASH_OFFSET
}

/// Computes one perspective-scaled billboard corner offset in screen pixels.
///
/// `size` is the unsigned perspective scale and `cornerAngle` uses 4096 units
/// per turn, zero upward. Requires positive `block->depth`. Truncates the
/// half-diagonal before multiplying by Q12 sine and cosine; changes only the
/// two offset words. Products use signed word arithmetic and must fit.
static inline void _diverComputeSparkCornerOffset(EffectBillboardScratch* block, u16 size, s32 cornerAngle)
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

    scratch                                = SCRATCH_STACK_CURSOR_SLOT;
    scratchHead                            = SCRATCH_STACK_CURSOR_AT(scratch, EffectBillboardScratch);
    block                                  = scratchHead - 1;
    depthOutput                            = &block->depth;
    block->worldPoint.vx                   = (u16)coord->workm.t[0];
    block->worldPoint.vy                   = (u16)coord->workm.t[1];
    block->worldPoint.vz                   = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(scratch, void) = block;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BYTES_AT(scratch, sizeof(*block));
}
