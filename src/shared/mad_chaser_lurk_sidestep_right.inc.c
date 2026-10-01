/* Part of the Mad Chaser library; see mad_chaser.h. */

/// The same side-step as `madChaserLurkSidestepToCombat`, but once the hit
/// flags are set it requests animation 3 and advances the sub-state instead.
void madChaserLurkSidestepRight(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            cond;
    s16            angle;
    s16            speed;
    s32            scale;

    work = (MadChaserWork*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                 = 0x1E;
        angle                                 = work->field_7A + 0x400;
        speed                                 = (((MadChaserWork*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work2 = (MadChaserWork*)arg0->work;
    if ((work2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (MadChaserWork*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}
