/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Falls along the heading in `field_40C`: moves the root 0xC8 units a frame,
/// adds the accelerating drop `field_42A` to its Y and to the second hit
/// body, and once the root reaches the ground height saved in `field_92`
/// requests landing animation 0x13 and advances the sub-state.
void madChaserLeapRebound(Task* arg0)
{
    MadChaserWork* work;
    s16            angle;
    GfxCoord*      coord;
    MadChaserWork* anim;
    s32            speed;
    s32            dx;

    work                                  = (MadChaserWork*)arg0->work;
    angle                                 = work->field_40C;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = 0xC8;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->obj_2CC.pos.vy                 += work->field_42A;
    work->field_428                      += 0xE;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                 = (MadChaserWork*)arg0->work;
        anim->field_426      = 2;
        anim->field_41C      = 0x10;
        anim->field_418      = 0x13;
        anim->field_414      = 1;
        coord->coord.t[1]    = (s16)work->field_92;
        work->obj_2CC.pos.vy = 0;
        work->field_412      = 0;
        work->field_422++;
    }
}
