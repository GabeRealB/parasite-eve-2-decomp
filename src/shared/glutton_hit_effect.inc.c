#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Scratch-stack block holding the two records a Glutton hit effect is spawned from.
///
/// Both are arguments of the id-dispatched effect spawner. Reserve one
/// complete block and release it once the effect is spawned; nothing in it is
/// meant to outlive the call.
typedef struct {
    SVECTOR        offset;    // Where the effect appears: an offset from the struck body's coordinate, along that coordinate's axes
    EffectSpawnArg effectArg; // Spawner's argument record: the struck body's coordinate, with spawn-argument halves 0x500 and 3
} _GluttonHitEffectScratch;
STATIC_ASSERT_SIZEOF(_GluttonHitEffectScratch, 0x10);

/// Spawn the hit effect for attack `id` on `coord`. The effect kind comes from
/// the attack's param 1; its offset from `coord` from param 0: kinds 2, 4, 6
/// and 7 use one fixed offset, every other kind draws one of three off
/// `gRandomLcgState`. The offset and the effect argument live in a block
/// borrowed from the scratchpad stack for the duration of the call.
void gluttonHitEffect(GfxCoord* coord, s32 id)
{
    _GluttonHitEffectScratch* scratch = SCRATCH_STACK_RESERVE_BLOCK(_GluttonHitEffectScratch);

    scratch->effectArg.spawnArgLo = 0x500;
    scratch->effectArg.coord      = coord;
    scratch->effectArg.spawnArgHi = 3;

    switch (Gp_GetIdParam0(id) & 0xFFFF) {
        case 2:
        case 4:
        case 6:
        case 7:
            scratch->offset.vx = 0;
            scratch->offset.vy = -0x190;
            scratch->offset.vz = 0x258;
            func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &scratch->offset, &scratch->effectArg);
            break;
        case 0:
        case 1:
        case 3:
        case 5:
        case 8:
        case 9:
        default:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            switch ((u16)((gRandomLcgState >> 16) % 3U)) {
                case 0:
                    scratch->offset.vy = 0;
                    scratch->offset.vx = 0;
                    scratch->offset.vz = 0x384;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 1:
                    scratch->offset.vx = 0x258;
                    scratch->offset.vy = -0xC8;
                    scratch->offset.vz = 0x2BC;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 2:
                    scratch->offset.vx = -0x12C;
                    scratch->offset.vy = -0x320;
                    scratch->offset.vz = 0x320;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &scratch->offset, &scratch->effectArg);
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GluttonHitEffectScratch);
}
