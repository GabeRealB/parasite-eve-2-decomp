/* Part of the Rat library; see rat.h. */

/// Returns the rat's aim error with a single-turn wrap adjustment.
///
/// `targetYaw` is in 0..4095; `currentYaw` is the u16 encoding of the stored
/// heading, including a turn step just outside that range. Angles use 4096
/// units per turn. The low signed halfword chooses the arc, while the wrapped
/// arc uses the full subtraction before narrowing the result. Normalized
/// inputs give 0..2048; a heading just outside the range can give a negative
/// error when the target changes across the wrap. That retained value still
/// passes the caller's one-sided aim test. No work or coordinate is changed.
static __inline__ s16 _ratGetAimError(u16 targetYaw, u16 currentYaw)
{
    s32 rawYawDelta;
    s16 yawDelta;
    s32 absYawDelta;
    s32 aimError;

    rawYawDelta = targetYaw - currentYaw;
    yawDelta    = rawYawDelta;
    absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;
    if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        aimError = absYawDelta;
    } else if (yawDelta > 0) {
        aimError = ACTOR_TRANSFORM_ANGLE_TURN - rawYawDelta;
    } else {
        aimError = rawYawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
    }
    return aimError;
}

/// Approaches the selected target, bites, then retreats and rolls a repeat.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`; a missing target borrows
/// the live player's root; an existing target must remain live. Positions share
/// the roots' parent frame; heading uses signed low-halfword X/Z offsets, while
/// range uses the full offsets. Yaw uses 4096 units per turn. The attack sphere pairs only during hit frames
/// until contact or the animation ends. The approach timeout is handled before
/// range/aim checks, so those checks still run on its expiry frame.
static void _ratAttack(Task* actor)
{
    enum {
        RAT_ATTACK_REACH           = 700,
        RAT_ATTACK_AIM_TOLERANCE   = 50,
        RAT_ATTACK_HIT_START_FRAME = 20,
        RAT_ATTACK_HIT_END_FRAME   = 32,
        RAT_ATTACK_BACK_OFF_FRAMES = 11,
        RAT_ATTACK_BACK_OFF_SPEED  = 120,
        RAT_ATTACK_RECOVER_FRAME   = 31,
    };

    VECTOR*    targetOffset;
    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    GfxCoord*  targetCoord;
    s32        distance;
    s32        backOffSpeed;
    s32        audioPan;
    s32        soundId;

    targetOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    model        = actor->extra.tmd;
    rootCoord    = model->coords;
    switch (work->step) {
        case RAT_ATTACK_STEP_APPROACH:
            sceneEngageBattle(1);
            if (work->targetCoord == 0) {
                work->targetCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            }
            targetCoord      = work->targetCoord;
            targetOffset->vx = targetCoord->coord.t[0] - rootCoord->coord.t[0];
            targetOffset->vy = 0;
            targetOffset->vz = targetCoord->coord.t[2] - rootCoord->coord.t[2];
            work->targetYaw  = ratan2((s16)targetOffset->vx, (s16)targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->turnRate   = RAT_WANDER_TURN_RATE;
            // Expiry resets idle state before the same frame still tests range and aim.
            work->timer--;
            if (work->timer <= 0) {
                work->mode   = RAT_MODE_IDLE;
                work->step   = RAT_IDLE_STEP_REST;
                work->animId = RAT_ANIM_IDLE;
                work->timer  = 0;
            }
            distance = SquareRoot0(targetOffset->vx * targetOffset->vx + targetOffset->vz * targetOffset->vz);
            if (distance < RAT_ATTACK_REACH) {
                if (_ratGetAimError(work->targetYaw, (u16)work->yaw) < RAT_ATTACK_AIM_TOLERANCE) {
                    work->animId       = RAT_ANIM_ATTACK;
                    work->forwardSpeed = 0;
                    work->turnRate     = 0;
                    work->step         = RAT_ATTACK_STEP_BITE;
                } else {
                    work->forwardSpeed = 0;
                }
            } else {
                work->forwardSpeed = RAT_RUN_SPEED;
            }
            break;

        case RAT_ATTACK_STEP_BITE:
            if (work->animFrame == RAT_ATTACK_HIT_START_FRAME) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame >= RAT_ATTACK_HIT_END_FRAME) {
                work->animId            = RAT_ANIM_BACK_OFF;
                work->step              = RAT_ATTACK_STEP_RETREAT;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;

        case RAT_ATTACK_STEP_RETREAT:
            backOffSpeed = 0;
            if (work->animFrame < RAT_ATTACK_BACK_OFF_FRAMES) {
                backOffSpeed = -RAT_ATTACK_BACK_OFF_SPEED;
            }
            work->forwardSpeed = backOffSpeed;
            if (work->animFrame >= RAT_ATTACK_RECOVER_FRAME) {
                soundId  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_ATTACK_RECOVER;
                audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < gRatAttackRepeatChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                    work->step   = RAT_ATTACK_STEP_APPROACH;
                    work->animId = RAT_ANIM_RUN;
                } else {
                    work->mode            = RAT_MODE_IDLE;
                    work->step            = RAT_IDLE_STEP_REST;
                    work->animId          = RAT_ANIM_IDLE;
                    work->timer           = 0;
                    work->wanderTimer     = 0;
                    work->attackRequested = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
