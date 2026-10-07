/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Walks a placement-sized patrol leg, listens at its end, and turns back.
///
/// `actor` is a live GOLEM body task. The placement variant sets each leg's
/// distance in multiples of 1000 game units; movement begins after the animation
/// blend. Player noise or another GOLEM's death interrupts the patrol and joins
/// battle. Angles use 4096 units per turn.
static void _golemPawnRookPatrolState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_TURN_AROUND     = 3,
        GOLEM_PAWN_ROOK_PATROL_WALK          = 0,
        GOLEM_PAWN_ROOK_PATROL_LISTEN        = 1,
        GOLEM_PAWN_ROOK_PATROL_TURN          = 2,
        GOLEM_PAWN_ROOK_PATROL_SPEED         = 20,
        GOLEM_PAWN_ROOK_PATROL_LISTEN_FRAMES = 96,
        GOLEM_PAWN_ROOK_PATROL_TURN_FRAMES   = 35,
        GOLEM_PAWN_ROOK_PATROL_TURN_RATE     = 59,
        GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES = 16,
        GOLEM_PAWN_ROOK_PATROL_DISTANCE_UNIT = 1000,
    };

    Enemy*             enemy;
    GolemPawnRookWork* work;
    GfxCoord*          root;
    s16                step;
    s16                speed;
    s32                yaw;
    s32                patrolLengthMultiplier;

    // This reservation has no accessed fields; its original role is unproven.
    SCRATCH_STACK_RESERVE_BYTES(GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES);

    root  = actor->extra.tmd->coords;
    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    step  = work->step;

    switch (step) {
        case GOLEM_PAWN_ROOK_PATROL_WALK:
            speed = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                speed = GOLEM_PAWN_ROOK_PATROL_SPEED;
            }
            work->forwardSpeed        = speed;
            work->turnRate            = 0;
            work->patrolDistanceLeft -= work->forwardSpeed;
            if (work->patrolDistanceLeft <= 0) {
                work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                work->timer        = 0;
                work->step         = GOLEM_PAWN_ROOK_PATROL_LISTEN;
                work->forwardSpeed = 0;
            }
            _golemPawnRookCheckPlayerNoise(actor);
            break;
        case GOLEM_PAWN_ROOK_PATROL_LISTEN:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->animFrame >= GOLEM_PAWN_ROOK_PATROL_LISTEN_FRAMES) {
                if (work->patrolDistanceLeft <= 0) {
                    patrolLengthMultiplier   = enemy->place->variant;
                    work->anim               = GOLEM_PAWN_ROOK_ANIM_TURN_AROUND;
                    work->step               = GOLEM_PAWN_ROOK_PATROL_TURN;
                    work->patrolDistanceLeft = patrolLengthMultiplier * GOLEM_PAWN_ROOK_PATROL_DISTANCE_UNIT;
                    yaw                      = ratan2(root->coord.m[0][2], root->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
                    work->yaw                = yaw;
                    work->targetYaw          = (yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->step = GOLEM_PAWN_ROOK_PATROL_WALK;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_PATROL_TURN:
            work->forwardSpeed = 0;
            work->turnRate     = GOLEM_PAWN_ROOK_PATROL_TURN_RATE;
            if (work->animFrame >= GOLEM_PAWN_ROOK_PATROL_TURN_FRAMES) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step = GOLEM_PAWN_ROOK_PATROL_WALK;
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

    SCRATCH_STACK_RELEASE_BYTES(GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES);
}
