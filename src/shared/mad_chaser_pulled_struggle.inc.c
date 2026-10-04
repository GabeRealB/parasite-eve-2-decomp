/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sub-state handler, the steering counterpart of `madChaserPulledLimp`:
/// while the enemy lives it turns `rotation.vy` toward `pullPoint` and pushes the
/// root back along it, then slides toward `pullPoint` accelerating with
/// `moveSpeed`. Past 120 frames (or once dead) it eases y in and marks
/// `busy`; while alive a hit flag plays sound 1. Within 800 units it
/// advances `subState`, otherwise a dead enemy queues its follow-up animation.
void madChaserPulledStruggle(Task* arg0)
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
    s32            dist;
    s32            cond;
    s32            soundId;
    s32            pan;

    obj   = arg0->extra.tmd;
    work  = (MadChaserWork*)arg0->work;
    coord = obj->coords;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work->stateFrames++;
    if (enemy->hp > 0) {
        MadChaserWork* w;
        s32            diff;
        s32            k;
        s32            step;

        if ((u32)(work->stateScratch >> 1) < 0x40) {
            work->animRate = work->stateScratch >> 2;
            work->stateScratch++;
        } else {
            work->animRate = 0x40;
        }
        w      = (MadChaserWork*)arg0->work;
        c      = arg0->extra.tmd->coords;
        dir.vx = work->pullPoint.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->pullPoint.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->rotation.vy - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->rotation.vy -= 0x18;
        } else if (diff < -0x100) {
            w->rotation.vy += 0x18;
        }
        angle                                 = work->rotation.vy;
        k                                     = -0x10;
        step                                  = ((((MadChaserWork*)arg0->work)->animRate * k) << 12) >> 16;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    {
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
    }
    d.vx = coord->coord.t[0] - work->pullPoint.vx;
    d.vy = coord->coord.t[1] - work->pullPoint.vy;
    d.vz = coord->coord.t[2] - work->pullPoint.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->stateFrames > 120) {
        if (dist <= 3000) {
            work->busy         = 1;
            coord->coord.t[1] += (work->pullPoint.vy - coord->coord.t[1]) >> 4;
        } else {
            work->busy         = 1;
            coord->coord.t[1] += (work->pullPoint.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->hp > 0) {
        MadChaserWork* w2 = (MadChaserWork*)arg0->work;

        if ((w2->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (w2->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
            pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->busy         = 1;
        coord->coord.t[1] += (work->pullPoint.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
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
                s16            next;

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
