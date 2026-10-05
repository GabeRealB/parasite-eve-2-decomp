/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sub-state handler: slides the model's root toward `pullPoint` in x/z,
/// accelerating with `moveSpeed`; after 90 frames it also eases y in and marks
/// `busy`. Within 800 units it advances `subState`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `busy` when
/// state 4 is pending).
void madChaserPulledLimp(Task* arg0)
{
    TmdObject*     obj;
    MadChaserWork* work;
    Enemy*         enemy;
    GfxCoord*      coord;
    GfxCoord*      c;
    VECTOR         d;
    SVECTOR        dir;
    VECTOR         sq;
    VECTOR*        out;
    s16            angle;
    s16            next;

    obj   = arg0->extra.tmd;
    work  = (MadChaserWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    coord = obj->coords;
    work->stateFrames++;
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    if ((s16)work->stateFrames < 0x5A) {
        s32 step = work->moveSpeed >> 6;

        c      = arg0->extra.tmd->coords;
        dir.vx = work->pullPoint.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->pullPoint.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        s32 step;

        work->busy = 1;
        step       = work->moveSpeed >> 5;
        c          = arg0->extra.tmd->coords;
        dir.vx     = work->pullPoint.vx - c->coord.t[0];
        dir.vy     = 0;
        dir.vz     = work->pullPoint.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]                    += (work->pullPoint.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->pullPoint.vx;
    d.vy = coord->coord.t[1] - work->pullPoint.vy;
    d.vz = coord->coord.t[2] - work->pullPoint.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        if (work->hitReaction != MAD_CHASER_HIT_REACTION_BLAST) {
            work->busy = 1;
            if (work->animId == 8) {
                if (work->hasLeaped == 0) {
                    MadChaserWork* w = (MadChaserWork*)arg0->work;

                    w->animBlendFrames = 4;
                    w->animRate        = ANIMATION_RATE_ONE;
                    w->animId          = 5;
                    w->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
                } else {
                    MadChaserWork* w = (MadChaserWork*)arg0->work;

                    w->animBlendFrames = 4;
                    w->animRate        = ANIMATION_RATE_ONE;
                    w->animId          = 6;
                    w->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
                }
            } else {
                MadChaserWork* w;

                next               = gMadChaserSettleAnims[work->animId - 1];
                w                  = (MadChaserWork*)arg0->work;
                w->animBlendFrames = 4;
                w->animRate        = ANIMATION_RATE_ONE;
                w->animId          = next;
                w->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            }
        } else {
            work->busy = 0;
        }
    }
}
