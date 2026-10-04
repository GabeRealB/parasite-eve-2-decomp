/* Part of the Rat library; see rat.h. */

/// Behaviour mode 2: stores the normalised player-to-enemy direction, plays
/// animation 0xA and for 15 frames pushes the root 50 units away from the
/// player while spinning the heading by 0x5C7 a frame on frames 6-14. When the
/// random 15-46 frame timer runs out it enters mode 3 if the build-up reaction
/// flag is set, otherwise plays animation 9 and returns to mode 0 with the
/// sensor flag latched at frame 0x20.
void ratStagger(Task* arg0)
{
    VECTOR     vec;
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    s32        state;
    s32        one;
    s32        rng;
    s32        posX;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->step;
    coord = obj->coords;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    work->animId        = RAT_ANIM_STAGGER;
    work->appliedAnimId = one;
    work->forwardSpeed  = 0;
    work->turnRate      = 0;
    work->knockedDown   = one;
    work->step          = one;
    rng                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->timer         = (((u32)rng >> 16) & 0x1F) + 0xF;
    gRandomLcgState     = rng;
    posX                = coord->coord.t[0];
    vec.vx              = gPlayerStatus.coordMtx->t[0] - posX;
    vec.vy              = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    vec.vz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    VectorNormalS(&vec, &work->staggerDir);
    goto pop;
case1:
    if (work->animFrame >= 0xF) {
        goto tick;
    }
    coord->coord.t[0] += -(work->staggerDir.vx * 50) >> 12;
    coord->coord.t[2] += -(work->staggerDir.vz * 50) >> 12;
tick:
    if (work->animFrame >= 6 && work->animFrame < 0xF) {
        work->turnRate  = 0x93;
        work->targetYaw = (work->targetYaw + 0x5C7) & 0xFFF;
    } else {
        work->turnRate = 0;
    }
    work->timer--;
    if (work->timer > 0) {
        goto pop;
    }
    if ((((Enemy*)arg0->spawnArg2.pointer)->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
        work->animId = RAT_ANIM_BUILDUP_HOLD;
        work->mode   = RAT_MODE_BUILDUP;
        work->step   = 3;
        goto pop;
    }
    work->animId = RAT_ANIM_STAGGER_RECOVER;
    work->step   = 2;
    goto pop;
case2:
    if (work->animFrame < 0x20) {
        goto pop;
    }
    work->mode            = RAT_MODE_IDLE;
    work->step            = 0;
    work->animId          = one;
    work->timer           = 0;
    work->attackRequested = one;
    work->knockedDown     = 0;
pop:;
}
