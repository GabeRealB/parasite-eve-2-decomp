/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Cancels a Beam Sword charge into stagger and disarms its strike contact.
///
/// Stops movement and damage accumulation without selecting the stagger clip.
/// A nonzero lowerShield clears its protection marker: run and slash use it,
/// while an interrupted wind-up retains that marker. work stays body-owned.
static inline void _golemPawnRookInterruptCharge(GolemPawnRookWork* work, s32 lowerShield)
{
    work->behavior     = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
    work->step         = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
    work->forwardSpeed = 0;
    work->turnRate     = 0;
    work->attackActive = 0;
    if (lowerShield) {
        work->shieldRaised = 0;
    }
    work->strikeBody.flags = work->strikeBody.flags &
                             (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Runs the Beam Sword charge, slash, and close-range follow-up selection.
///
/// `actor` is a live Beam Sword GOLEM body task. Distances are horizontal game
/// units and headings use 4096 units per turn, with player offsets narrowed to
/// signed halfwords for aiming. The Pawn steers during the charge; the Rook holds
/// its heading. Sufficient damage before the strike interrupts into stagger.
static void _golemPawnRookSwordChargeState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_SWORD_SWING             = 8,
        GOLEM_PAWN_ROOK_ANIM_SWORD_CHARGE_SLASH      = 7,
        GOLEM_PAWN_ROOK_ANIM_SWORD_RUN               = 6,
        GOLEM_PAWN_ROOK_CHARGE_WINDUP                = 0,
        GOLEM_PAWN_ROOK_CHARGE_RUN                   = 1,
        GOLEM_PAWN_ROOK_CHARGE_SLASH                 = 2,
        GOLEM_PAWN_ROOK_CHARGE_ADVANCE_FRAME         = 71,
        GOLEM_PAWN_ROOK_CHARGE_STOP_RANGE            = 1000,
        GOLEM_PAWN_ROOK_CHARGE_SPEED                 = 132,
        GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_START_FRAME = 70,
        GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_HP          = 76,
        GOLEM_PAWN_ROOK_CHARGE_WINDUP_FRAMES         = 82,
        GOLEM_PAWN_ROOK_CHARGE_SLASH_RANGE           = 1500,
        GOLEM_PAWN_ROOK_CHARGE_MAX_YAW_ERROR         = 256,
        GOLEM_PAWN_ROOK_CHARGE_STRIKE_START_FRAME    = 13,
        GOLEM_PAWN_ROOK_CHARGE_STRIKE_END_FRAME      = 30,
        GOLEM_PAWN_ROOK_CHARGE_SLASH_FRAMES          = 59,
        GOLEM_PAWN_ROOK_CHARGE_FOLLOWUP_RANGE        = 3000,
        GOLEM_PAWN_ROOK_CHARGE_ATTACK                = 1,
    };

    s16                step;
    s16                yawDifference;
    s16                yawError;
    s32                windupDx;
    s32                windupDz;
    s32                chargeDx;
    s32                chargeDz;
    s32                followupDx;
    s32                followupDz;
    s32                playerDistance;
    s32                soundId;
    s32                audioPan;
    GolemPawnRookWork* work;
    GfxCoord*          root;
    VECTOR*            toPlayer;

    toPlayer = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work     = actor->work;
    step     = work->step;
    root     = actor->extra.tmd->coords;
    // Damage can interrupt the wind-up, run, or pre-contact portion of the slash.
    switch (step) {
        case GOLEM_PAWN_ROOK_CHARGE_WINDUP:
            if (work->animFrame >= GOLEM_PAWN_ROOK_CHARGE_ADVANCE_FRAME) {
                work->shieldRaised = (work->shieldHp > 0);
                toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
                windupDz           = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
                toPlayer->vz       = windupDz;
                windupDx           = toPlayer->vx;
                if (SquareRoot0((windupDx * windupDx) + (windupDz * windupDz)) >= GOLEM_PAWN_ROOK_CHARGE_STOP_RANGE) {
                    work->forwardSpeed = GOLEM_PAWN_ROOK_CHARGE_SPEED;
                } else {
                    work->forwardSpeed = 0;
                }
            } else {
                work->forwardSpeed = 0;
            }
            work->turnRate = 0;
            if (work->animFrame == GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_START_FRAME) {
                work->interruptDamage = 0;
                work->attackActive    = 1;
            }
            if ((work->animFrame >= GOLEM_PAWN_ROOK_CHARGE_ADVANCE_FRAME) && (work->interruptDamage >= GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_HP)) {
                _golemPawnRookInterruptCharge(work, false);
                break;
            }
            if (work->animFrame >= (gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_CHARGE_WINDUP_FRAMES)) {
                work->step = GOLEM_PAWN_ROOK_CHARGE_RUN;
                work->anim = GOLEM_PAWN_ROOK_ANIM_SWORD_RUN;
            }
            break;
        case GOLEM_PAWN_ROOK_CHARGE_RUN:
            work->forwardSpeed = GOLEM_PAWN_ROOK_CHARGE_SPEED;
            work->turnRate     = GOLEM_PAWN_ROOK_CHARGE_TURN;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            toPlayer->vz       = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            work->targetYaw    = ratan2((s16)toPlayer->vx, (s16)toPlayer->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            chargeDx           = toPlayer->vx;
            chargeDz           = toPlayer->vz;
            playerDistance     = SquareRoot0((chargeDx * chargeDx) + (chargeDz * chargeDz));
            if (work->interruptDamage >= GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_HP) {
                _golemPawnRookInterruptCharge(work, true);
                break;
            }
            if (playerDistance < GOLEM_PAWN_ROOK_CHARGE_SLASH_RANGE) {
                work->step         = GOLEM_PAWN_ROOK_CHARGE_SLASH;
                work->anim         = GOLEM_PAWN_ROOK_ANIM_SWORD_CHARGE_SLASH;
                work->forwardSpeed = 0;
            } else {
                yawDifference = (ratan2((s16)toPlayer->vx, (s16)toPlayer->vz) & ACTOR_TRANSFORM_ANGLE_MASK) - work->yaw;
                yawError      = (abs(yawDifference) >= ACTOR_TRANSFORM_ANGLE_HALF_TURN) ? ((yawDifference > 0) ? ACTOR_TRANSFORM_ANGLE_TURN - yawDifference : yawDifference + ACTOR_TRANSFORM_ANGLE_TURN) : abs(yawDifference);
                if (yawError > GOLEM_PAWN_ROOK_CHARGE_MAX_YAW_ERROR) {
                    work->behavior     = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step         = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
                    work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                    work->attackActive = 0;
                    work->shieldRaised = 0;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_CHARGE_SLASH:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if ((work->animFrame < GOLEM_PAWN_ROOK_CHARGE_STRIKE_START_FRAME) && (work->interruptDamage >= GOLEM_PAWN_ROOK_CHARGE_INTERRUPT_HP)) {
                _golemPawnRookInterruptCharge(work, true);
                break;
            }
            if (work->animFrame == GOLEM_PAWN_ROOK_CHARGE_STRIKE_START_FRAME) {
                work->strikeBody.flags = work->strikeBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->strikeBody.key   = damagePackAttackKey(gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_CHARGE_ATTACK);
                soundId                = gGolemPawnRookSwingCue | ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                audioPan               = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame == GOLEM_PAWN_ROOK_CHARGE_STRIKE_END_FRAME) {
                work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame >= GOLEM_PAWN_ROOK_CHARGE_SLASH_FRAMES) {
                work->shieldRaised = 0;
                followupDx         = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
                toPlayer->vx       = followupDx;
                followupDz         = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
                toPlayer->vz       = followupDz;
                if (SquareRoot0((followupDx * followupDx) + (followupDz * followupDz)) < GOLEM_PAWN_ROOK_CHARGE_FOLLOWUP_RANGE) {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_SWORD_SWING;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step     = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
