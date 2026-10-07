#include "main/random.h"

/* Part of the Diver library; see diver.h. */

/// Spawns the impact burst at `coord`. Kind 0 is a lone room effect. Kind 1 is
/// the spark plus a puff (when bit 0 of `phase` is clear) and a randomly aimed
/// spark every eighth phase. Kind 2 is the spark, a flash and a ring of four
/// aimed sparks. `arg3`'s low 12 bits are the effect size and bits 12..15 its
/// variant, and `(phase >> 1) % 6` picks the spark frame. While effects are
/// paused only the spark is drawn.
void diverImpactBurst(GfxCoord* coord, u16 arg1, u16 arg2, u32 arg3)
{
    SVECTOR vec;
    s32     i;
    u16     variant;
    u16     param;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        diverDrawSpark(coord, ((u32)arg1 >> 1) % 6, 0x400, 0);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    }

    variant = (arg3 >> 12) & 0xF;
    param   = arg3 & 0xFFF;

    switch (arg2) {
        case 0:
            effectSpawn(gRoomEffectWaterSprayId, coord, 0x14001000 + param + variant, NULL);
            break;

        case 1:
            diverDrawSpark(coord, ((u32)arg1 >> 1) % 6, param, 0);
            if (!(arg1 & 1)) {
                effectSpawn(gRoomEffectWaterSprayId, coord, 0x01000000 + param + variant, NULL);
            }
            if (!(arg1 & 7)) {
                SVECTOR* dir;

                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vx          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vy          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vz          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)(param), dir);
            }
            break;

        case 2:
            diverDrawSpark(coord, ((u32)arg1 >> 1) % 6, param, 0);
            effectSpawn(gRoomEffectWaterSprayId, coord, 0x10001000 + param + variant, NULL);
            for (i = 0; i < 4; i++) {
                SVECTOR* dir;

                effectSpawn(gRoomEffectWaterSprayId, coord, 0x02001000 + param + variant, NULL);

                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vx          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vy          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                vec.vz          = 0x80 - ((gRandomLcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                effectSpawn(EFFECT_FLASH_BURST, coord, (s32)(param), dir);
            }
            break;
    }
}
