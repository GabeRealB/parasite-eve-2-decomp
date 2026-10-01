#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Spawn the hit effect for attack `id` on `coord`. The effect kind comes from
/// the attack's param 1; its rotation from param 0: kinds 2, 4, 6 and 7 use one
/// fixed rotation, every other kind draws one of three off `gRandomLcgState`. The
/// rotation and the effect argument live in a block borrowed from the
/// scratchpad stack for the duration of the call.
void gluttonHitEffect(GfxCoord* coord, s32 id)
{
    Actor403200EffScratch* sc = (Actor403200EffScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200EffScratch));

    sc->eff.spawnArgLo = 0x500;
    sc->eff.coord      = coord;
    sc->eff.spawnArgHi = 3;

    switch (Gp_GetIdParam0(id) & 0xFFFF) {
        case 2:
        case 4:
        case 6:
        case 7:
            sc->rot.vx = 0;
            sc->rot.vy = -0x190;
            sc->rot.vz = 0x258;
            func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
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
                    sc->rot.vy = 0;
                    sc->rot.vx = 0;
                    sc->rot.vz = 0x384;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 1:
                    sc->rot.vx = 0x258;
                    sc->rot.vy = -0xC8;
                    sc->rot.vz = 0x2BC;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 2:
                    sc->rot.vx = -0x12C;
                    sc->rot.vy = -0x320;
                    sc->rot.vz = 0x320;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200EffScratch));
}
