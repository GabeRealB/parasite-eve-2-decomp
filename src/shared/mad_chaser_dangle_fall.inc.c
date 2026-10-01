/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Drops the model back to the ground: eases the pitch latched in
/// `field_434` back to zero while keeping the heading, adds the accelerating
/// drop to the root Y, and on landing requests animation 0xC and advances the
/// sub-state.
void madChaserDangleFall(Task* arg0)
{
    MadChaserWork* work;
    GfxCoord*      coord;
    OverlayMat     rot;
    OverlayMat*    src;
    MATRIX*        dst;
    MadChaserWork* anim;

    work              = (MadChaserWork*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    src               = &rot;
    src->ident.m00M01 = ONE;
    src->ident.m02M10 = 0;
    src->ident.m11M12 = ONE;
    src->ident.m20M21 = 0;
    src->ident.m22    = ONE;
    work->field_434  += -work->field_434 >> 2;
    RotMatrixX(work->field_434, &src->mat);
    RotMatrixY(work->field_7A, &src->mat);
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
    work->field_428   += 2;
    work->field_42A   += work->field_428;
    coord->coord.t[1] += work->field_42A;
    if (coord->coord.t[1] > 0) {
        work->field_412   = 0;
        coord->coord.t[1] = 0;
        anim              = (MadChaserWork*)arg0->work;
        anim->field_41C   = 0x20;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_422++;
    }
}
