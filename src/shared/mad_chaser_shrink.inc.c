/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Death shrink without the effect of `madChaserShrinkWithDust`:
/// restores the saved root matrix, scales it on Y by `shrinkScaleY` (0x40
/// smaller each frame), sets light mode 2 on frame 16, and after frame 32
/// hides the model, clears the frame counter and advances the state.
void madChaserShrink(Task* arg0)
{
    MadChaserWork* work;
    TmdObject*     obj;
    GfxCoord*      coord;
    VECTOR         scale;
    MATRIX         m;

    work                = (MadChaserWork*)arg0->work;
    obj                 = arg0->extra.tmd;
    coord               = obj->coords;
    work->shrinkScaleY -= 0x40;
    scale.vx            = 0x1000;
    scale.vy            = (s16)work->shrinkScaleY;
    scale.vz            = 0x1000;
    coord->coord        = work->savedRootMtx;
    gfxSetRotIdentity(&m);
    ScaleMatrix(&m, &scale);
    MulMatrix(&coord->coord, &m);
    if ((s16)++work->stateFrames == 0x10) {
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if ((s16)work->stateFrames > 0x20) {
        obj->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->stateFrames = 0;
        work->state++;
    }
}
