/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Turns model parts 5, 4 and 3 about Y by a third of `spineYaw` each: reads
/// each part's rotation back as Euler angles, adds to the yaw, rebuilds the
/// 3x3 and marks the coordinate dirty.
void madChaserTwistSpine(Task* arg0)
{
    SVECTOR        rot;
    MATRIX         mtx;
    MadChaserWork* work;
    GfxCoord*      coords;
    MATRIX*        m5;
    MATRIX*        m4;
    MATRIX*        m3;

    work   = (MadChaserWork*)arg0->work;
    coords = arg0->extra.tmd->coords;

    gfxSetRotIdentity(&mtx);
    m5 = &coords[5].coord;
    gfxExtractEulerAngles(m5, &rot);
    rot.vy = (u16)rot.vy + work->spineYaw / 3;
    RotMatrix(&rot, &mtx);
    m5->m[0][0]            = (u16)mtx.m[0][0];
    m5->m[0][1]            = (u16)mtx.m[0][1];
    m5->m[0][2]            = (u16)mtx.m[0][2];
    m5->m[1][0]            = (u16)mtx.m[1][0];
    m5->m[1][1]            = (u16)mtx.m[1][1];
    m5->m[1][2]            = (u16)mtx.m[1][2];
    m5->m[2][0]            = (u16)mtx.m[2][0];
    m5->m[2][1]            = (u16)mtx.m[2][1];
    m5->m[2][2]            = (u16)mtx.m[2][2];
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;

    gfxSetRotIdentity(&mtx);
    m4 = &coords[4].coord;
    gfxExtractEulerAngles(m4, &rot);
    rot.vy = (u16)rot.vy + work->spineYaw / 3;
    RotMatrix(&rot, &mtx);
    m4->m[0][0]            = (u16)mtx.m[0][0];
    m4->m[0][1]            = (u16)mtx.m[0][1];
    m4->m[0][2]            = (u16)mtx.m[0][2];
    m4->m[1][0]            = (u16)mtx.m[1][0];
    m4->m[1][1]            = (u16)mtx.m[1][1];
    m4->m[1][2]            = (u16)mtx.m[1][2];
    m4->m[2][0]            = (u16)mtx.m[2][0];
    m4->m[2][1]            = (u16)mtx.m[2][1];
    m4->m[2][2]            = (u16)mtx.m[2][2];
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;

    gfxSetRotIdentity(&mtx);
    m3 = &coords[3].coord;
    gfxExtractEulerAngles(m3, &rot);
    rot.vy = (u16)rot.vy + work->spineYaw / 3;
    RotMatrix(&rot, &mtx);
    m3->m[0][0]            = (u16)mtx.m[0][0];
    m3->m[0][1]            = (u16)mtx.m[0][1];
    m3->m[0][2]            = (u16)mtx.m[0][2];
    m3->m[1][0]            = (u16)mtx.m[1][0];
    m3->m[1][1]            = (u16)mtx.m[1][1];
    m3->m[1][2]            = (u16)mtx.m[1][2];
    m3->m[2][0]            = (u16)mtx.m[2][0];
    m3->m[2][1]            = (u16)mtx.m[2][1];
    m3->m[2][2]            = (u16)mtx.m[2][2];
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
}
