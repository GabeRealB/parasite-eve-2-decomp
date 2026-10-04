/* Part of the library; see glutton.h. Inline helpers the fragments use. */

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// shrinks it uniformly to half size: `ratan2` of the rotation's Z basis gives
/// the yaw, `gfxRotMatrixY` rebuilds the rotation from it and `ScaleMatrix`
/// applies 0.5 on all three axes. The working matrix lives in a frame carved
/// off the scratch stack, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_RebuildRotation` in `actor_444000_4.c`.
static __inline__ void gluttonShrinkRotation(GfxCoord* coord)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = 0x800;
    sc->scale.vy = 0x800;
    sc->scale.vz = 0x800;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it: `ratan2` of the rotation's Z basis gives the yaw,
/// `gfxRotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `xz` on both horizontal axes and `y` on the vertical one. The working
/// matrix lives in a frame carved off the scratch stack, which is handed back
/// once the rotation has been copied onto the coordinate. Written as an inline
/// so the four scratch-head accesses stay absolute, like
/// `gluttonShrinkRotation` above.
static __inline__ void gluttonScaleRotation(GfxCoord* coord, s16 xz, s32 y)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = xz;
    sc->scale.vy = y;
    sc->scale.vz = xz;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Horizontal gap from `coord` to the player's coordinate matrix `gPlayerStatus.coordMtx`, as an
/// `SVECTOR` the caller supplies.
static __inline__ void gluttonGapToCamera(GfxCoord* coord, SVECTOR* out)
{
    out->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    out->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    out->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
}
