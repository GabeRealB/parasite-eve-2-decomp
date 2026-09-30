/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Death shrink without the effect of `hopperShrinkWithDust`:
/// restores the saved root matrix, scales it on Y by `field_430` (0x40
/// smaller each frame), sets light mode 2 on frame 16, and after frame 32
/// hides the model, clears the frame counter and advances the state.
void hopperShrink(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    VECTOR           scale;
    OverlayMat       m;
    GpMtxWords*      ident;

    work             = (Actor341700Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra.tmd;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->savedRootMtx;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_412 = 0;
        work->field_420++;
    }
}
