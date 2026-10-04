/* Part of the Rat library; see rat.h. */

/// Behaviour mode 1: state 0 latches the target coordinate (the player task if
/// none), turns toward it and, inside 0x2BC, starts the attack animation once
/// the heading is within 0x32 (otherwise stops), approaching at 0x32 farther
/// out; a timeout returns to mode 0. State 1 enables the attack sphere from
/// animation frame 0x14 and disables it at 0x20; state 2 backs off at -0x78 for
/// 11 frames, then plays sound 4 and rolls gRatAttackRepeatChance by place row
/// to attack again or return to mode 0.
void ratAttack(Task* arg0)
{
    VECTOR*    vec;
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    GfxCoord*  target;
    s32        state;
    s32        one;
    s32        dist;
    s32        raw;
    s16        diff;
    s32        adiff;
    s32        ang;
    s32        vel;
    s32        pan;
    s32        snd;

    one   = 1;
    vec   = (VECTOR*)SCRATCH_STACK_RESERVE_BYTES(0x10);
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
    Gp_ArmStateF0(1);
    if (work->targetCoord == 0) {
        work->targetCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    }
    target          = work->targetCoord;
    vec->vx         = target->coord.t[0] - coord->coord.t[0];
    vec->vy         = 0;
    vec->vz         = target->coord.t[2] - coord->coord.t[2];
    work->targetYaw = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
    work->turnRate  = 0x19;
    work->timer--;
    if (work->timer > 0) {
        goto dist;
    }
    work->mode   = RAT_MODE_IDLE;
    work->step   = 0;
    work->animId = one;
    work->timer  = 0;
dist:
    dist = SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz);
    if (dist < 0x2BC) {
        raw   = work->targetYaw - (u16)work->yaw;
        diff  = raw;
        adiff = diff >= 0 ? diff : -diff;
        if (adiff < 0x800) {
            ang = adiff;
            goto wrap_done;
        }
        if (diff > 0) {
            ang = 0x1000 - raw;
            goto wrap_done;
        }
        ang = raw + 0x1000;
    wrap_done:
        if ((s16)ang < 0x32) {
            work->animId       = RAT_ANIM_ATTACK;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->step         = 1;
            goto pop;
        }
        work->forwardSpeed = 0;
        goto pop;
    }
    work->forwardSpeed = 0x32;
    goto pop;
case1:
    if (work->animFrame == 0x14) {
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->animFrame < 0x20) {
        goto pop;
    }
    work->animId            = RAT_ANIM_BACK_OFF;
    work->step              = 2;
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    goto pop;
case2:
    vel = 0;
    if (work->animFrame < 0xB) {
        vel = -0x78;
    }
    work->forwardSpeed = vel;
    if (work->animFrame < 0x1F) {
        goto pop;
    }
    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070004;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((s32)((gRandomLcgState >> 16) & 0xF) < gRatAttackRepeatChance[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex]) {
        work->step   = 0;
        work->animId = state;
        goto pop;
    }
    work->mode            = RAT_MODE_IDLE;
    work->step            = 0;
    work->animId          = one;
    work->timer           = 0;
    work->wanderTimer     = 0;
    work->attackRequested = 0;
pop:
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
