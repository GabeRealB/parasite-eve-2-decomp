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
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    work->field_37E = 0xA;
    work->field_380 = one;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_396 = one;
    work->field_37C = one;
    rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_38C = (((u32)rng >> 16) & 0x1F) + 0xF;
    gRandomLcgState = rng;
    posX            = coord->coord.t[0];
    vec.vx          = gPlayerStatus.coordMtx->t[0] - posX;
    vec.vy          = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    vec.vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    VectorNormalS(&vec, &work->field_370);
    goto pop;
case1:
    if ((s16)work->field_382 >= 0xF) {
        goto tick;
    }
    coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
    coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
tick:
    if ((u32)(work->field_382 - 6) < 9) {
        work->field_386 = 0x93;
        work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
    } else {
        work->field_386 = 0;
    }
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto pop;
    }
    if ((((Enemy*)arg0->spawnArg2.pointer)->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
        work->field_37E = 8;
        work->field_37A = 3;
        work->field_37C = 3;
        goto pop;
    }
    work->field_37E = 9;
    work->field_37C = 2;
    goto pop;
case2:
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_394 = one;
    work->field_396 = 0;
pop:;
}
