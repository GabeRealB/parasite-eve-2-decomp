/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Flies backwards off the heading, pitching toward 0x200, under the
/// accelerating drop; on landing levels out, requests animation 0xC,
/// launches again and advances the state.
void hopperEmergeArcBack(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GfxCoord*        coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                  = (Actor341700Work*)arg0->work;
    angle                                 = work->field_7A;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = -0x8C;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_78                       += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 2;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}
