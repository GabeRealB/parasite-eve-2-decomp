/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Turns model parts 5, 4 and 3 about Y by a third of `field_424` each: reads
/// each part's rotation back as Euler angles, adds to the yaw, rebuilds the
/// 3x3 and marks the coordinate dirty.
void hopperTwistSpine(Task* arg0)
{
    SVECTOR          rot;
    OverlayMat       mtx;
    GpMtxWords*      ident;
    Actor341700Work* work;
    GfxCoord*        coords;
    MATRIX*          m5;
    MATRIX*          m4;
    MATRIX*          m3;

    work   = (Actor341700Work*)arg0->work;
    ident  = &mtx.ident;
    coords = arg0->extra.tmd->coords;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]            = (u16)mtx.mat.m[0][0];
    m5->m[0][1]            = (u16)mtx.mat.m[0][1];
    m5->m[0][2]            = (u16)mtx.mat.m[0][2];
    m5->m[1][0]            = (u16)mtx.mat.m[1][0];
    m5->m[1][1]            = (u16)mtx.mat.m[1][1];
    m5->m[1][2]            = (u16)mtx.mat.m[1][2];
    m5->m[2][0]            = (u16)mtx.mat.m[2][0];
    m5->m[2][1]            = (u16)mtx.mat.m[2][1];
    m5->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]            = (u16)mtx.mat.m[0][0];
    m4->m[0][1]            = (u16)mtx.mat.m[0][1];
    m4->m[0][2]            = (u16)mtx.mat.m[0][2];
    m4->m[1][0]            = (u16)mtx.mat.m[1][0];
    m4->m[1][1]            = (u16)mtx.mat.m[1][1];
    m4->m[1][2]            = (u16)mtx.mat.m[1][2];
    m4->m[2][0]            = (u16)mtx.mat.m[2][0];
    m4->m[2][1]            = (u16)mtx.mat.m[2][1];
    m4->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]            = (u16)mtx.mat.m[0][0];
    m3->m[0][1]            = (u16)mtx.mat.m[0][1];
    m3->m[0][2]            = (u16)mtx.mat.m[0][2];
    m3->m[1][0]            = (u16)mtx.mat.m[1][0];
    m3->m[1][1]            = (u16)mtx.mat.m[1][1];
    m3->m[1][2]            = (u16)mtx.mat.m[1][2];
    m3->m[2][0]            = (u16)mtx.mat.m[2][0];
    m3->m[2][1]            = (u16)mtx.mat.m[2][1];
    m3->m[2][2]            = (u16)mtx.mat.m[2][2];
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
}
