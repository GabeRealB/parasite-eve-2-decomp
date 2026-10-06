/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Death shrink: restores the root matrix saved in `savedRootMtx`, scales it
/// on Y by `shrinkScaleY` (0x40 smaller each frame), spawns effect 0x600A5 on
/// frame 4, sets the enemy's light mode 2 on frame 16, and after frame 32
/// hides the model and advances the state.
void madChaserShrinkWithDust(Task* arg0)
{
    MadChaserWork*    work;
    TmdObject*        obj;
    GfxCoord*         coord;
    VECTOR            scale;
    GfxMatrix         m;
    GfxRotationWords* ident;
    SVECTOR           ofs;

    work                   = (MadChaserWork*)arg0->work;
    ident                  = &m.rotationWords;
    obj                    = arg0->extra.tmd;
    coord                  = obj->coords;
    work->shrinkScaleY    -= 0x40;
    scale.vx               = 0x1000;
    scale.vy               = (s16)work->shrinkScaleY;
    scale.vz               = 0x1000;
    coord->coord           = work->savedRootMtx;
    m.rotationWords.m00M01 = ONE;
    m.rotationWords.m02M10 = 0;
    ident->m11M12          = ONE;
    m.rotationWords.m20M21 = 0;
    ident->m22             = ONE;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->stateFrames == 4) {
        ofs.vx = 0;
        ofs.vy = 0;
        ofs.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 3, &ofs);
    }
    if ((s16)work->stateFrames == 0x10) {
        worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if ((s16)work->stateFrames > 0x20) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}
