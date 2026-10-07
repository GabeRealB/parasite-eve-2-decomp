/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Rebuilds the root rotation: pitches about X by a sine sway driven by
/// `frameCount`, turns by the heading `rotation.vy`, and copies the 3x3 into the
/// root coordinate. When `hitTaken` is 1, latches that pitch into
/// `fallPitch`, releases the anchor, clears the flag and the fall motion, and advances the
/// sub-state.
void madChaserDangleSway(Task* arg0)
{
    MadChaserWork* work;
    GfxCoord*      coord;
    GfxMatrix      rot;
    GfxMatrix*     src;
    MATRIX*        dst;
    s16            pitch;

    work                      = (MadChaserWork*)arg0->work;
    coord                     = arg0->extra.tmd->coords;
    src                       = &rot;
    gfxSetRotIdentity(&src->mat);
    pitch                     = ((rsin(work->frameCount << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    RotMatrixY(work->rotation.vy, &src->mat);
    dst          = &coord->coord;
    dst->m[0][0] = src->mat.m[0][0];
    dst->m[0][1] = src->mat.m[0][1];
    dst->m[0][2] = src->mat.m[0][2];
    dst->m[1][0] = src->mat.m[1][0];
    dst->m[1][1] = src->mat.m[1][1];
    dst->m[1][2] = src->mat.m[1][2];
    dst->m[2][0] = src->mat.m[2][0];
    dst->m[2][1] = src->mat.m[2][1];
    dst->m[2][2] = src->mat.m[2][2];
    if (work->hitTaken == 1) {
        work->hitTaken  = 0;
        work->anchored  = 0;
        work->moveAccel = 0;
        work->moveSpeed = 0;
        work->fallPitch = pitch;
        work->subState++;
    }
}
