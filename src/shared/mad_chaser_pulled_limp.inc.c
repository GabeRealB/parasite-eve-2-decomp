/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sub-state handler: slides the model's root toward `field_70` in x/z,
/// accelerating with `field_42A`; after 90 frames it also eases y in and marks
/// `field_438`. Within 800 units it advances `field_422`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `field_438` when
/// state 4 is pending).
void madChaserPulledLimp(Task* arg0)
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
    s16              next;

    obj   = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    coord = obj->coords;
    work->field_412++;
    work->field_428++;
    work->field_42A += work->field_428;
    if ((s16)work->field_412 < 0x5A) {
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
    } else {
        s32 step;

        work->field_438 = 1;
        step            = work->field_42A >> 5;
        c               = arg0->extra.tmd->coords;
        dir.vx          = work->field_70.vx - c->coord.t[0];
        dir.vy          = 0;
        dir.vz          = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                 = ratan2(dir.vx, dir.vz);
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * step) >> 16;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]                    += (work->field_70.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(0x402C0002, 1);
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

                next         = gMadChaserSettleAnims[work->field_418 - 1];
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
