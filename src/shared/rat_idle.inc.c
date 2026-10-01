/* Part of the Rat library; see rat.h. */

/// Behaviour mode 0: with the sensor sphere armed, every 30 frames rolls
/// gRatSlowMoveChance / gRatFastMoveChance (indexed by the place row) to start
/// a timed move at speed 0x14 (animation 7) or 0x32 (animation 2), with
/// durations from gRatSlowMoveTimes / gRatFastMoveTimes. Re-picks a random
/// heading within +-0x3FF of the current one every 0-31 frames; once the sensor
/// has fired it switches to mode 1, sets a 60-91 frame timer and plays sound 3.
/// Ends by ticking the idle sound.
void ratIdle(Task* arg0)
{
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    s32        state;
    s32        one;
    s32        rng0;
    s32        rng1;
    s32        rng2;
    s32        rng3;
    s32        rng4;
    s32        rng5;
    s32        rng6;
    s32        timer;
    s32        next;
    s32        flags;
    s32        ang;
    s32        snd;
    s32        pan;

    one   = 1;
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->field_37C;
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
    goto tail;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto tail;
case0:
    flags           = work->field_1FA;
    work->field_384 = 0;
    work->field_1FA = flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    timer           = work->field_38C + 1;
    work->field_38C = timer;
    if ((s16)timer < 0x1E) {
        goto tail;
    }
    rng0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rng0;
    if ((s32)(((u32)rng0 >> 16) & 0xF) <
        gRatSlowMoveChance[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->field_37E = 7;
        next            = gRatSlowMoveTimes[((u32)(rng1 = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2            = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        gRatFastMoveChance[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->field_37E = 2;
        next            = gRatFastMoveTimes[((u32)(rng3 = rng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState = rng3;
        work->field_37C = 2;
        work->field_38C = next;
        goto tail;
    }
    work->field_38C = 0;
    goto tail;
case1:
    work->field_384 = 0x14;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
    goto tail;
case2:
    work->field_384 = 0x32;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
tail:
    work->field_38E = work->field_38E - 1;
    if ((s16)work->field_38E > 0) {
        goto post;
    }
    work->field_386 = 0x19;
    rng4            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rng5            = rng4 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    ang             = ((u32)rng5 >> 16) & 0x3FF;
    gRandomLcgState = rng4;
    work->field_38E = ((u32)rng4 >> 16) & 0x1F;
    gRandomLcgState = rng5;
    if ((((u32)rng5 >> 16) & 0x400) == 0) {
        ang = -ang;
    }
    work->field_38A = ((u16)work->field_388 + ang) & 0xFFF;
post:
    if (work->field_394 != 0) {
        work->field_37A = 1;
        work->field_394 = 0;
        work->field_37C = 0;
        work->field_37E = 2;
        rng6            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_38C = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070003;
        gRandomLcgState = rng6;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    ratIdleSound(arg0);
}
