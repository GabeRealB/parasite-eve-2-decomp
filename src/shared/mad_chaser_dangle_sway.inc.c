/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Replaces the root's nine Q12 rotation coefficients for the anchored sway.
///
/// Both matrices must be live through the call. Copies only the 3x3 basis;
/// translation and the matrix's alignment halfword are retained. The caller
/// is responsible for invalidating coordinate composition.
static __inline__ void _madChaserDangleSwayCopyRotation(MATRIX* rootMatrix, const MATRIX* rotationMatrix)
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

/// Sways the anchored root until a hit releases it into the fall.
///
/// Requires live work/model storage. Replaces root rotation with X pitch then
/// Y heading, retaining translation. Pitch uses 4096 angle units per turn:
/// a 64-frame sine cycle of amplitude 512 about -1024. Only hitTaken == 1
/// releases the anchor, consumes the hit latch, saves pitch and zeros the
/// signed-halfword fall motion before advancing. The caller marks composition
/// dirty and applies the part-6 anchor while it remains enabled.
static void _madChaserDangleSway(Task* task)
{
    enum {
        MAD_CHASER_DANGLE_PHASE_SHIFT      = 6,
        MAD_CHASER_DANGLE_SWAY_SCALE       = 16,
        MAD_CHASER_DANGLE_SWAY_SCALE_SHIFT = 7,
    };
    MadChaserWork* work;
    GfxCoord*      root;
    MATRIX         rotation;
    MATRIX*        rotationMatrix;
    MATRIX*        rootMatrix;
    s16            pitch;

    work           = task->work;
    root           = task->extra.tmd->coords;
    rotationMatrix = &rotation;
    gfxSetRotIdentity(rotationMatrix);
    pitch = ((rsin(work->frameCount << MAD_CHASER_DANGLE_PHASE_SHIFT) * MAD_CHASER_DANGLE_SWAY_SCALE) >> MAD_CHASER_DANGLE_SWAY_SCALE_SHIFT) - ACTOR_TRANSFORM_ANGLE_TURN / 4;
    RotMatrixX(pitch, rotationMatrix);
    RotMatrixY(work->rotation.vy, rotationMatrix);
    rootMatrix = &root->coord;
    _madChaserDangleSwayCopyRotation(rootMatrix, rotationMatrix);
    if (work->hitTaken == 1) {
        work->hitTaken  = 0;
        work->anchored  = 0;
        work->moveAccel = 0;
        work->moveSpeed = 0;
        work->fallPitch = pitch;
        work->subState++;
    }
}
