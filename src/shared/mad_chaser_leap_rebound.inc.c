/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Falls along the heading in `leapHeading`: moves the root 0xC8 units a frame,
/// adds the accelerating drop `moveSpeed` to its Y and to the second hit
/// body, and once the root reaches the ground height saved in `moveStartPos.vy`
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
    angle                                 = work->leapHeading;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = 0xC8;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->moveSpeed;
    work->gridBody.pos.vy                += work->moveSpeed;
    work->moveAccel                      += 0xE;
    work->moveSpeed                      += work->moveAccel;
    if (coord->coord.t[1] >= work->moveStartPos.vy) {
        anim                  = (MadChaserWork*)arg0->work;
        anim->animBlendFrames = 2;
        anim->animRate        = ANIMATION_RATE_ONE;
        anim->animId          = 0x13;
        anim->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        coord->coord.t[1]     = work->moveStartPos.vy;
        work->gridBody.pos.vy = 0;
        work->stateFrames     = 0;
        work->subState++;
    }
}
