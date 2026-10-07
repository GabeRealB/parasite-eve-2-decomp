#include "gameplay/room_effects.h"

/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Picks one of twelve hit positions out of `gDesertChaserHitOffsets` by damage
/// magnitude `arg1`, then spawns effect `damageGetPlayerAttackEffectId(arg2)` on the model
/// part that entry names.
void desertChaserHitEffect(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*          sc;
    s32               mag;
    DesertChaserWork* work;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = gDesertChaserHitOffsets[0];
                break;
            case 1:
                *sc = gDesertChaserHitOffsets[1];
                break;
            case 2:
                *sc = gDesertChaserHitOffsets[2];
                break;
            case 3:
                *sc = gDesertChaserHitOffsets[3];
                break;
            default:
                *sc = gDesertChaserHitOffsets[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = gDesertChaserHitOffsets[5];
                break;
            case 1:
                *sc = gDesertChaserHitOffsets[6];
                break;
            default:
                *sc = gDesertChaserHitOffsets[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = gDesertChaserHitOffsets[8];
        } else {
            *sc = gDesertChaserHitOffsets[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = gDesertChaserHitOffsets[10];
        } else {
            *sc = gDesertChaserHitOffsets[11];
        }
    }
    work->effectArg.coord      = &arg0->extra.tmd->coords[sc->pad];
    work->effectArg.spawnArgLo = 0x100;
    work->effectArg.spawnArgHi = 2;
    work->hitOffset            = *sc;
    effectSpawnHit(damageGetPlayerAttackEffectId(arg2), &arg0->extra.tmd->coords[sc->pad], &work->hitOffset, &work->effectArg);
    SCRATCH_STACK_RELEASE_BYTES(8);
}
