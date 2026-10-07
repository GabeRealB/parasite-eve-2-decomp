#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Scatters the body when the enemy bursts: a chunk of model 0 from joint 6,
/// then one of model 1 from joint 8 or model 2 from joint 2 at random (effect
/// 0x20010, each taking the enemy's texture page and CLUT row), and effect
/// 0x60030 at joints 1, 3 and 4.
void madChaserSpawnGibs(Task* arg0)
{
    EffectWork* eff;
    EffectWork* eff2;
    TmdObject*  dst;
    TmdObject*  dst2;
    TmdObject*  src;
    TmdObject*  src2;

    D_800678F0[0] = &gMadChaserChunkModel0;
    eff           = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[6], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_800678F0[0] = &gMadChaserChunkModel1;
        eff2          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = &gMadChaserChunkModel2;
        eff2          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdBuildBufferHalf(dst2);
            tmdBuildBufferHalf(dst2);
        }
    }
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[3], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[4], 0x200, NULL);
}
