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

/// Chooses a random flash offset of approximately 64 strike-local units.
///
/// `vector` is a writable SVECTOR lvalue and `vectorPtr` a writable SVECTOR*
/// local; both must be side-effect-free and occur repeatedly. Assigns XYZ
/// directly before taking the address, leaves pad untouched, advances the
/// shared LCG three times and normalizes/scales on the GTE. A zero direction
/// is passed to the SDK without a guard. Captures the enclosing radius constant.
/// Expands to statements; invoke only within a braced block.
#define DIVER_RANDOMIZE_FLASH_OFFSET(vector, vectorPtr)                               \
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
    (vector).vx     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);                        \
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
    (vector).vy     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);                        \
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
    (vector).vz     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);                        \
    (vectorPtr)     = &(vector);                                                      \
    VectorNormalSS((vectorPtr), (vectorPtr));                                         \
    gte_lddp(DIVER_FLASH_OFFSET_RADIUS);                                              \
    gte_ldsv(vectorPtr);                                                              \
    gte_gpf12();                                                                      \
    gte_stsv(vectorPtr);

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

                DIVER_RANDOMIZE_FLASH_OFFSET(flashOffset, offsetPtr);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)size, offsetPtr);
            }
            break;

        case DIVER_BURST_IMPACT:
            _diverDrawSpark(coord, ((u32)phase >> DIVER_BURST_PHASES_PER_CELL_SHIFT) % DIVER_SPARK_FRAME_COUNT, size, 0);
            effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_IMPACT_UPRIGHT_PARTICLE + size + spraySizeBias, NULL);
            for (flashIndex = 0; flashIndex < DIVER_BURST_FLASH_COUNT; flashIndex++) {
                SVECTOR* offsetPtr;

                effectSpawn(gRoomEffectWaterSprayId, coord, DIVER_BURST_IMPACT_SPRAY_PARTICLE + size + spraySizeBias, NULL);
                DIVER_RANDOMIZE_FLASH_OFFSET(flashOffset, offsetPtr);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)size, offsetPtr);
            }
            break;
    }
#undef DIVER_RANDOMIZE_FLASH_OFFSET
}
