/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Flies backwards off the heading, pitching up toward 0x800, under the
/// accelerating drop `moveSpeed`; on landing turns around, requests
/// animation 0x11, launches again and advances the state.
void madChaserEmergeBackflip(Task* arg0)
{
    MadChaserWork* work;
    s16            angle;
    GfxCoord*      coord;
    MadChaserWork* anim;
    s32            speed;
    s32            dx;

    work                                  = (MadChaserWork*)arg0->work;
    angle                                 = work->rotation.vy;
    coord                                 = arg0->extra.tmd->coords;
    dx                                    = rsin(angle) << 4;
    speed                                 = -0x8C;
    arg0->extra.tmd->coords->coord.t[0]  += (dx * speed) >> 16;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 16;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->rotation.vx                    += (0x800 - work->rotation.vx) >> 3;
    coord->coord.t[1]                    += work->moveSpeed;
    work->moveAccel                      += 2;
    work->moveSpeed                      += work->moveAccel;
    if (coord->coord.t[1] > 0) {
        work->shadowHidden = 0;
        work->stateFrames  = 0;
        coord->coord.t[1]  = -0x3C;
        work->rotation.vx  = 0;
        work->rotation.vz  = 0;
        work->rotation.vy += 0x800;
        anim               = (MadChaserWork*)arg0->work;
        anim->animRate     = ANIMATION_RATE_ONE;
        anim->animId       = 0x11;
        anim->animRequest  = MAD_CHASER_ANIM_REQUEST_RESET;
        work->moveAccel    = 0;
        work->moveSpeed    = -0x6E;
        work->state++;
    }
}
