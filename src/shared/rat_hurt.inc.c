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
    state = work->field_37C;
    coord = obj->coords;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->field_37E = 5;
    work->field_380 = 1;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_37C = 1;
    snd             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070002;
    pan             = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
case1:
    if ((s16)work->field_382 < 0x18) {
        return;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = state;
    work->field_38C = 0;
    work->field_394 = state;
}
