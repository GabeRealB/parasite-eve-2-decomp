/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Rebuilds the root rotation: pitches about X by a sine sway driven by
/// `field_442`, turns by the heading `field_7A`, and copies the 3x3 into the
/// root coordinate. When `field_41E` is 1, latches that pitch into
/// `field_434`, clears the flag and three motion halfwords, and advances the
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
    src->rotationWords.m00M01 = ONE;
    src->rotationWords.m02M10 = 0;
    src->rotationWords.m11M12 = ONE;
    src->rotationWords.m20M21 = 0;
    src->rotationWords.m22    = ONE;
    pitch                     = ((rsin(work->field_442 << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    RotMatrixY(work->field_7A, &src->mat);
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
    if (work->field_41E == 1) {
        work->field_41E = 0;
        work->field_432 = 0;
        work->field_428 = 0;
        work->field_42A = 0;
        work->field_434 = pitch;
        work->field_422++;
    }
}
