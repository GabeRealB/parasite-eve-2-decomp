/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Drops the model back to the ground: eases the pitch latched in
/// `fallPitch` back to zero while keeping the heading, adds the accelerating
/// drop to the root Y, and on landing requests animation 0xC and advances the
/// sub-state.
void madChaserDangleFall(Task* arg0)
{
    MadChaserWork* work;
    GfxCoord*      coord;
    GfxMatrix      rot;
    GfxMatrix*     src;
    MATRIX*        dst;
    MadChaserWork* anim;

    work  = (MadChaserWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    src   = &rot;
    gfxSetRotIdentity(&src->mat);
    work->fallPitch += -work->fallPitch >> 2;
    RotMatrixX(work->fallPitch, &src->mat);
    RotMatrixY(work->rotation.vy, &src->mat);
    dst                = &coord->coord;
    dst->m[0][0]       = src->mat.m[0][0];
    dst->m[0][1]       = src->mat.m[0][1];
    dst->m[0][2]       = src->mat.m[0][2];
    dst->m[1][0]       = src->mat.m[1][0];
    dst->m[1][1]       = src->mat.m[1][1];
    dst->m[1][2]       = src->mat.m[1][2];
    dst->m[2][0]       = src->mat.m[2][0];
    dst->m[2][1]       = src->mat.m[2][1];
    dst->m[2][2]       = src->mat.m[2][2];
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] > 0) {
        work->stateFrames = 0;
        coord->coord.t[1] = 0;
        anim              = (MadChaserWork*)arg0->work;
        anim->animRate    = 0x20;
        anim->animId      = 0xC;
        anim->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        work->subState++;
    }
}
