#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Gives a detached chunk the body's texture placement and rebuilds its buffer.
///
/// Both tasks must own live models; chunkEffect is a successful model-effect
/// spawn. Copies only texture-page and CLUT-row offsets. An existing chunk
/// buffer has both halves rebuilt; an absent buffer is retained as absent.
static __inline__ void _madChaserBindGibTexture(Task* task, EffectWork* chunkEffect)
{
    TmdObject* bodyModel  = task->extra.tmd;
    TmdObject* chunkModel = chunkEffect->task->extra.tmd;

    chunkModel->texturePageOffset = bodyModel->texturePageOffset;
    chunkModel->clutRowOffset     = bodyModel->clutRowOffset;
    if (chunkModel->buffer != NULL) {
        tmdBuildBufferHalf(chunkModel);
        tmdBuildBufferHalf(chunkModel);
    }
}

/// Spawns two detached model chunks and three gravity particles from the body.
///
/// Requires a live model with at least nine coordinates and the carrier's
/// three chunk sources loaded. Publishes the head source for a synchronous
/// body-chunk spawn at part 6, then chooses the arm at part 8 or tail at part 2
/// using one LCG advance. Successful chunk models inherit texture-page and
/// CLUT-row offsets; both halves of an existing primitive buffer are rebuilt.
/// Particles spawn at parts 1, 3 and 4 even if a chunk allocation fails. Effects
/// own their new tasks and snapshot placement; the chunk sources must stay
/// loaded until those models are released. The body is borrowed for this call.
static void _madChaserSpawnGibs(Task* task)
{
    // Low spawn-argument bits size particles, including the chunks' trail puffs.
    enum { MAD_CHASER_GIB_PARTICLE_SIZE = 0x200 };
    EffectWork* headEffect;
    EffectWork* secondaryEffect;

    D_800678F0[0] = &gMadChaserChunkModel0;
    headEffect    = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[6], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    if (headEffect != NULL) {
        _madChaserBindGibTexture(task, headEffect);
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_800678F0[0]   = &gMadChaserChunkModel1;
        secondaryEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[8], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    } else {
        D_800678F0[0]   = &gMadChaserChunkModel2;
        secondaryEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[2], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    }
    if (secondaryEffect != NULL) {
        _madChaserBindGibTexture(task, secondaryEffect);
    }
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[3], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[4], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
}
