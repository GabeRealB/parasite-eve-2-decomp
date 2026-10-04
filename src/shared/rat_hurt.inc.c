/* Part of the Rat library; see rat.h. */

/// Behaviour mode 4: stops, plays animation 5 and sound 2, then at animation
/// frame 0x18 returns to mode 0 with the sensor flag latched.
void ratHurt(Task* arg0)
{
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    s32        state;
    s32        snd;
    s32        pan;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->step;
    coord = obj->coords;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->animId        = RAT_ANIM_HURT;
    work->appliedAnimId = RAT_ANIM_IDLE;
    work->forwardSpeed  = 0;
    work->turnRate      = 0;
    work->step          = 1;
    snd                 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070002;
    pan                 = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
case1:
    if (work->animFrame < 0x18) {
        return;
    }
    work->mode            = RAT_MODE_IDLE;
    work->step            = 0;
    work->animId          = state;
    work->timer           = 0;
    work->attackRequested = state;
}
