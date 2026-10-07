/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Copies the replacement dangle rotation while retaining root translation.
///
/// Requires readable source and writable destination 3x3 coefficients. The
/// matrices are borrowed; translation and composition stamps are retained.
static __inline__ void _madChaserDangleFallCopyRotation(MATRIX* rootMatrix, const MATRIX* rotationMatrix)
{
    rootMatrix->m[0][0] = rotationMatrix->m[0][0];
    rootMatrix->m[0][1] = rotationMatrix->m[0][1];
    rootMatrix->m[0][2] = rotationMatrix->m[0][2];
    rootMatrix->m[1][0] = rotationMatrix->m[1][0];
    rootMatrix->m[1][1] = rotationMatrix->m[1][1];
    rootMatrix->m[1][2] = rotationMatrix->m[1][2];
    rootMatrix->m[2][0] = rotationMatrix->m[2][0];
    rootMatrix->m[2][1] = rotationMatrix->m[2][1];
    rootMatrix->m[2][2] = rotationMatrix->m[2][2];
}

/// Drops the released root to parent Y zero while easing its saved pitch upright.
///
/// Requires live model/work storage and fall motion initialized by the sway.
/// Eases pitch by one quarter of its negation in 4096 angle units per turn,
/// retains heading, and replaces the root's rotation. Adds 2 to s16 acceleration
/// and then acceleration to s16 speed each frame; Y advances by that speed in
/// parent-coordinate units. Landing requires Y > 0: clamps it to zero, resets
/// the u16 frame counter, cuts to clip 12 at twice normal rate and advances.
/// The caller ticks animation and marks the root composition dirty.
static void _madChaserDangleFall(Task* task)
{
    enum {
        MAD_CHASER_DANGLE_PITCH_EASE_SHIFT = 2,
        MAD_CHASER_DANGLE_FALL_ACCEL       = 2,
        MAD_CHASER_DANGLE_LAND_CLIP        = 12,
    };
    MadChaserWork* work;
    GfxCoord*      root;
    MATRIX         rotation;
    MATRIX*        rotationMatrix;
    MATRIX*        rootMatrix;
    MadChaserWork* requestWork;

    work           = task->work;
    root           = task->extra.tmd->coords;
    rotationMatrix = &rotation;
    gfxSetRotIdentity(rotationMatrix);
    work->fallPitch += -work->fallPitch >> MAD_CHASER_DANGLE_PITCH_EASE_SHIFT;
    RotMatrixX(work->fallPitch, rotationMatrix);
    RotMatrixY(work->rotation.vy, rotationMatrix);
    rootMatrix = &root->coord;
    _madChaserDangleFallCopyRotation(rootMatrix, rotationMatrix);
    // Signed halfword motion accumulates before the strict floor crossing test.
    work->moveAccel  += MAD_CHASER_DANGLE_FALL_ACCEL;
    work->moveSpeed  += work->moveAccel;
    root->coord.t[1] += work->moveSpeed;
    if (root->coord.t[1] > 0) {
        work->stateFrames        = 0;
        root->coord.t[1]         = 0;
        requestWork              = task->work;
        requestWork->animRate    = 2 * ANIMATION_RATE_ONE;
        requestWork->animId      = MAD_CHASER_DANGLE_LAND_CLIP;
        requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        work->subState++;
    }
}
