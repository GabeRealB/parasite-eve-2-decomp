/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Alternates standing idle and listening until the player is spotted or battle is alerted.
///
/// `actor` is a live GOLEM body task. The idle phase checks player noise;
/// listening waits for its animation. Either a spotted player or another
/// GOLEM's death starts the engage behaviour and joins battle.
static void _golemPawnRookIdleState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_IDLE          = 1,
        GOLEM_PAWN_ROOK_IDLE_WAIT          = 0,
        GOLEM_PAWN_ROOK_IDLE_LISTEN        = 1,
        GOLEM_PAWN_ROOK_IDLE_FRAMES        = 91,
        GOLEM_PAWN_ROOK_IDLE_LISTEN_FRAMES = 94,
    };

    GolemPawnRookWork* work;
    s16                step;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_IDLE_WAIT:
            work->timer++;
            if (work->timer >= GOLEM_PAWN_ROOK_IDLE_FRAMES) {
                work->anim  = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                work->timer = 0;
                work->step  = GOLEM_PAWN_ROOK_IDLE_LISTEN;
            }
            _golemPawnRookCheckPlayerNoise(actor);
            break;
        case GOLEM_PAWN_ROOK_IDLE_LISTEN:
            if (work->animFrame >= GOLEM_PAWN_ROOK_IDLE_LISTEN_FRAMES) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_IDLE;
                work->step = GOLEM_PAWN_ROOK_IDLE_WAIT;
            }
            break;
    }

    if ((work->playerSpotted != 0) || (gSceneCombatState.golemPawnRookDeathAlert != 0)) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
        work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
        work->timer    = 0;
        sceneEngageBattle(1);
    }
}

/// Plays the standing stagger, then resumes engage or an interrupted buildup.
///
/// `task` is a live GOLEM body task. Movement stops at entry. The buildup marker
/// preserves a pending reaction across the stagger rather than restarting it.
static void _golemPawnRookStaggerState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_BUILDUP   = 0x14,
        GOLEM_PAWN_ROOK_ANIM_STAGGER   = 0x11,
        GOLEM_PAWN_ROOK_STAGGER_START  = 0,
        GOLEM_PAWN_ROOK_STAGGER_WAIT   = 1,
        GOLEM_PAWN_ROOK_STAGGER_FRAMES = 55,
    };

    GolemPawnRookWork* work;
    s16                step;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_STAGGER_START:
            work->anim         = GOLEM_PAWN_ROOK_ANIM_STAGGER;
            work->step         = GOLEM_PAWN_ROOK_STAGGER_WAIT;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case GOLEM_PAWN_ROOK_STAGGER_WAIT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_STAGGER_FRAMES) {
                if (work->buildupActive == 0) {
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                } else {
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_BUILDUP;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                }
            }
            break;
    }
}

/// Stops the body for a heavy hit recoil, then resumes player engagement.
///
/// The last hit's side chooses the clip: front recovers after 80 frames and behind
/// after 59. The actor must have live GOLEM work; movement and animation are
/// requests executed by the enclosing frame handler.
static void _golemPawnRookRecoilState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_RECOIL_START         = 0,
        GOLEM_PAWN_ROOK_RECOIL_FRONT         = 1,
        GOLEM_PAWN_ROOK_RECOIL_BEHIND        = 2,
        GOLEM_PAWN_ROOK_RECOIL_ANIM_FRONT    = 0x12,
        GOLEM_PAWN_ROOK_RECOIL_ANIM_BEHIND   = 0x13,
        GOLEM_PAWN_ROOK_RECOIL_FRONT_FRAMES  = 80,
        GOLEM_PAWN_ROOK_RECOIL_BEHIND_FRAMES = 59,
    };
    GolemPawnRookWork* work;
    s32                step;
    s32                hitFromFront;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_RECOIL_START:
            hitFromFront = work->hitFromFront;
            if (hitFromFront == 1) {
                work->anim = GOLEM_PAWN_ROOK_RECOIL_ANIM_FRONT;
                work->step = hitFromFront;
            } else {
                work->anim = GOLEM_PAWN_ROOK_RECOIL_ANIM_BEHIND;
                work->step = GOLEM_PAWN_ROOK_RECOIL_BEHIND;
            }
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case GOLEM_PAWN_ROOK_RECOIL_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_RECOIL_FRONT_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
        case GOLEM_PAWN_ROOK_RECOIL_BEHIND:
            if (work->animFrame >= GOLEM_PAWN_ROOK_RECOIL_BEHIND_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
    }
}

/// Ticks the buildup reaction to completion, then plays its recovery.
///
/// `task` is a live GOLEM body task with an active enemy buildup countdown.
/// Completion clears the GOLEM's buildup marker; the recovery animation then
/// returns to engage. The damage timer's grade and expiry rules belong to
/// `damageTickEnemyBuildup`.
static void _golemPawnRookBuildupState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_BUILDUP_RECOVER    = 0x13,
        GOLEM_PAWN_ROOK_BUILDUP_WAIT            = 0,
        GOLEM_PAWN_ROOK_BUILDUP_RECOVER         = 1,
        GOLEM_PAWN_ROOK_BUILDUP_RECOVERY_FRAMES = 59,
    };

    GolemPawnRookWork* work;
    s16                step;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_BUILDUP_WAIT:
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->anim          = GOLEM_PAWN_ROOK_ANIM_BUILDUP_RECOVER;
                work->step          = GOLEM_PAWN_ROOK_BUILDUP_RECOVER;
                work->buildupActive = 0;
            }
            break;
        case GOLEM_PAWN_ROOK_BUILDUP_RECOVER:
            if (work->animFrame >= GOLEM_PAWN_ROOK_BUILDUP_RECOVERY_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
    }
}

/// Plays a fatal reaction while downed, then hands the body task to its dead state.
///
/// `actor` is a live GOLEM body task. The saved downed pose selects the reaction
/// clip and its end frame; no movement or collision shape is changed here.
static void _golemPawnRookDownedDeathState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_DOWNED_DEATH_START  = 0,
        GOLEM_PAWN_ROOK_DOWNED_DEATH_BEHIND = 1,
        GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT  = 2,
    };

    GolemPawnRookWork* work;
    s32                downedPose;
    s16                step;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_START:
            // Keep the widened pose snapshot used again as the handler step.
            downedPose = work->downedPose;
            if (downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
                return;
            }
            work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_FRONT;
            work->step = GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT;
            return;
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_BEHIND:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND_FRAMES) {
                actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                work->step   = GOLEM_PAWN_ROOK_DOWNED_DEATH_START;
            }
            return;
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT_FRAMES) {
                actor->state = step;
                work->step   = GOLEM_PAWN_ROOK_DOWNED_DEATH_START;
            }
            return;
    }
}

/// Leaves the body unchanged for behavior slots unavailable in this carrier.
///
/// Used for sword or launcher handlers the other weapon lacks and for the Pawn's
/// absent Silence scream. The callback argument is unused.
static void _golemPawnRookNopState(Task* task)
{
}
