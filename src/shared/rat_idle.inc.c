/* Part of the Rat library; see rat.h. */

/// Behaviour mode 0: with the sensor sphere armed, every 30 frames rolls
/// gRatSlowMoveChance / gRatFastMoveChance (indexed by the place row) to start
/// a timed move at speed 0x14 (animation 7) or 0x32 (animation 2), with
/// durations from gRatSlowMoveTimes / gRatFastMoveTimes. Re-picks a random
/// heading within +-0x3FF of the current 1 every 0-31 frames; once the sensor
/// has fired it switches to mode 1, sets a 60-91 frame timer and plays sound 3.
/// Ends by ticking the idle sound.
void ratIdle(Task* arg0)
{
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    s32        state;
    s32        rng0;
    s32        rng1;
    s32        rng2;
    s32        rng3;
    s32        rng4;
    s32        rng5;
    s32        rng6;
    s32        next;
    s32        ang;
    s32        snd;
    s32        pan;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    state = work->step;
    coord = obj->coords;
    switch (state) {
        case 0:
            work->forwardSpeed      = 0;
            work->sensorBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->timer++;
            if (work->timer < 0x1E) {
                break;
            }
            rng0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng0;
            if ((s32)(((u32)rng0 >> 16) & 0xF) <
                gRatSlowMoveChance[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_WALK;
                next            = gRatSlowMoveTimes[((u32)(rng1 = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = rng1;
                work->step      = 1;
                work->timer     = next;
                break;
            }
            rng2            = rng0 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng2;
            if ((s32)(((u32)rng2 >> 16) & 0xF) <
                gRatFastMoveChance[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_RUN;
                next            = gRatFastMoveTimes[((u32)(rng3 = rng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = rng3;
                work->step      = 2;
                work->timer     = next;
                break;
            }
            work->timer = 0;
            break;
        case 1:
            work->forwardSpeed = 0x14;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = 0;
            break;
        case 2:
            work->forwardSpeed = 0x32;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = 0;
            break;
    }
    work->wanderTimer--;
    if (work->wanderTimer <= 0) {
        work->turnRate    = 0x19;
        rng4              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rng5              = rng4 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        ang               = ((u32)rng5 >> 16) & 0x3FF;
        gRandomLcgState   = rng4;
        work->wanderTimer = ((u32)rng4 >> 16) & 0x1F;
        gRandomLcgState   = rng5;
        if ((((u32)rng5 >> 16) & 0x400) == 0) {
            ang = -ang;
        }
        work->targetYaw = ((u16)work->yaw + ang) & 0xFFF;
    }
    if (work->attackRequested != 0) {
        work->mode            = RAT_MODE_ATTACK;
        work->attackRequested = 0;
        work->step            = 0;
        work->animId          = RAT_ANIM_RUN;
        rng6                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->timer           = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd                   = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070003;
        gRandomLcgState       = rng6;
        pan                   = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    ratIdleSound(arg0);
}
