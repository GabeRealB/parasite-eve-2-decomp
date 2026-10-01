/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Sub-state handler, the steering counterpart of `hopperPulledLimp`:
/// while the enemy lives it turns `field_7A` toward `field_70` and pushes the
/// root back along it, then slides toward `field_70` accelerating with
/// `field_42A`. Past 120 frames (or once dead) it eases y in and marks
/// `field_438`; while alive a hit flag plays sound 1. Within 800 units it
/// advances `field_422`, otherwise a dead enemy queues its follow-up animation.
void hopperPulledStruggle(Task* arg0)
{
    TmdObject*       obj;
    Actor341700Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s32              dist;
    s32              cond;
    s32              soundId;
    s32              pan;

    obj   = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    coord = obj->coords;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work->field_412++;
    if (enemy->hp > 0) {
        Actor341700Work* w;
        s32              diff;
        s32              k;
        s32              step;

        if ((u32)(work->field_44F >> 1) < 0x40) {
            work->field_41C = work->field_44F >> 2;
            work->field_44F++;
        } else {
            work->field_41C = 0x40;
        }
        w      = (Actor341700Work*)arg0->work;
        c      = arg0->extra.tmd->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->field_7A - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->field_7A -= 0x18;
        } else if (diff < -0x100) {
            w->field_7A += 0x18;
        }
        angle                                 = work->field_7A;
        k                                     = -0x10;
        step                                  = ((((Actor341700Work*)arg0->work)->field_41C * k) << 12) >> 16;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work->field_428++;
    work->field_42A += work->field_428;
    {
        s32 step = work->field_42A >> 6;

        c      = arg0->extra.tmd->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->field_412 > 120) {
        if (dist <= 3000) {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 4;
        } else {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->hp > 0) {
        Actor341700Work* w2 = (Actor341700Work*)arg0->work;

        if ((w2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (w2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
            pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->field_438    = 1;
        coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor341700Work* w = (Actor341700Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor341700Work* w;
                s16              next;

                next         = gHopperSettleAnims[work->field_418 - 1];
                w            = (Actor341700Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}
