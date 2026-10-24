#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Gives a detached chunk the body's texture placement and rebuilds its buffer.
///
/// Both tasks must own live models; chunkEffect is a successful model-effect
/// spawn. Copies signed texture-page offsets in 64-word VRAM columns and CLUT
/// row offsets. An existing chunk buffer has both halves rebuilt, preserving
/// its next-half selector; source/buffer capacities, scratch space and GPU
/// lifetime must satisfy `tmdBuildBufferHalf`. An absent buffer stays absent.
/// Borrows the task/effect handles; only the chunk model and buffer are changed.
static inline void _madChaserBindGibTexture(const Task* bodyTask, const EffectWork* chunkEffect)
{
    const TmdObject* bodyModel  = bodyTask->extra.tmd;
    TmdObject*       chunkModel = chunkEffect->task->extra.tmd;

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

    gTaskBank2Descs[TASK_BANK2_BURST_MODEL_PART_SLOT].data.model = &MAD_CHASER_BURST_HEAD_SOURCE;
    headEffect                                                   = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[MAD_CHASER_BURST_HEAD_BODY_PART], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    if (headEffect != NULL) {
        _madChaserBindGibTexture(task, headEffect);
    }
    RANDOM_LCG_ADVANCE();
    if (RANDOM_LCG_DRAW() & 1) {
        gTaskBank2Descs[TASK_BANK2_BURST_MODEL_PART_SLOT].data.model = &MAD_CHASER_BURST_ARM_SOURCE;
        secondaryEffect                                              = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[MAD_CHASER_BURST_ARM_BODY_PART], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    } else {
        gTaskBank2Descs[TASK_BANK2_BURST_MODEL_PART_SLOT].data.model = &MAD_CHASER_BURST_TAIL_SOURCE;
        secondaryEffect                                              = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[MAD_CHASER_BURST_TAIL_BODY_PART], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    }
    if (secondaryEffect != NULL) {
        _madChaserBindGibTexture(task, secondaryEffect);
    }
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[3], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[4], MAD_CHASER_GIB_PARTICLE_SIZE, NULL);
}

/// Distributes the look-around yaw across the three spine parts.
///
/// Requires live work and a model with at least six coordinates. Visits parts
/// 5, 4 and 3 in that order, extracts each local Euler rotation and adds one
/// third of signed spineYaw (4096 angle units per turn, division toward zero).
/// Narrows the added yaw back to a halfword and replaces the nine Q12 basis
/// coefficients, retaining translations and alignment bytes. Dirties each part;
/// SDK rotation routines change GTE state.
static void _madChaserTwistSpine(Task* task)
{
    /// Adds one third of look yaw to one joint and replaces its local basis.
    ///
    /// jointCoord is a writable GfxCoord lvalue, jointMatrix a MATRIX pointer
    /// lvalue, angles an SVECTOR lvalue, rotation a MATRIX lvalue and workBlock
    /// a live MadChaserWork pointer. angles and rotation must be separate from
    /// joint/work storage; jointMatrix receives the joint matrix address.
    /// Arguments occur repeatedly and must be stable and free of side effects.
    /// Expands to a compound statement, captures the local joint-count constant,
    /// and is undefined after this function's three calls. Retains translation
    /// and alignment bytes, narrows yaw to s16 and dirties joint composition.
#define MAD_CHASER_TWIST_SPINE_JOINT(jointCoord, jointMatrix, angles, rotation, workBlock)     \
    {                                                                                          \
        gfxSetRotIdentity(&(rotation));                                                        \
        (jointMatrix) = &(jointCoord).coord;                                                   \
        gfxExtractEulerAngles((jointMatrix), &(angles));                                       \
        (angles).vy = (u16)(angles).vy + (workBlock)->spineYaw / MAD_CHASER_SPINE_JOINT_COUNT; \
        RotMatrix(&(angles), &(rotation));                                                     \
        (jointMatrix)->m[0][0]    = (rotation).m[0][0];                                        \
        (jointMatrix)->m[0][1]    = (rotation).m[0][1];                                        \
        (jointMatrix)->m[0][2]    = (rotation).m[0][2];                                        \
        (jointMatrix)->m[1][0]    = (rotation).m[1][0];                                        \
        (jointMatrix)->m[1][1]    = (rotation).m[1][1];                                        \
        (jointMatrix)->m[1][2]    = (rotation).m[1][2];                                        \
        (jointMatrix)->m[2][0]    = (rotation).m[2][0];                                        \
        (jointMatrix)->m[2][1]    = (rotation).m[2][1];                                        \
        (jointMatrix)->m[2][2]    = (rotation).m[2][2];                                        \
        (jointCoord).composeStamp = GRAPHICS_COORD_DIRTY;                                      \
    }
    enum { MAD_CHASER_SPINE_JOINT_COUNT = 3 };

    SVECTOR        eulerAngles;
    MATRIX         rotationMatrix;
    MadChaserWork* work;
    GfxCoord*      coords;
    MATRIX*        part5Matrix;
    MATRIX*        part4Matrix;
    MATRIX*        part3Matrix;

    work   = task->work;
    coords = task->extra.tmd->coords;

    MAD_CHASER_TWIST_SPINE_JOINT(coords[5], part5Matrix, eulerAngles, rotationMatrix, work);
    MAD_CHASER_TWIST_SPINE_JOINT(coords[4], part4Matrix, eulerAngles, rotationMatrix, work);
    MAD_CHASER_TWIST_SPINE_JOINT(coords[3], part3Matrix, eulerAngles, rotationMatrix, work);
#undef MAD_CHASER_TWIST_SPINE_JOINT
}
