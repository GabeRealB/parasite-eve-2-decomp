/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Approach-cycle state (entry 1 of the package's behaviour table). State 0 drains the
/// `patrolDistanceLeft` budget by `forwardSpeed` (0 while `animFrame` is under the
/// animation's start frame in `gGolemPawnRookAnimBlendFrames`, 0x14 once past it) and runs
/// the proximity check `golemPawnRookCheckProximity` every frame; when the budget runs
/// out it switches to animation 4 and state 1. State 1 waits for `animFrame`
/// to reach 0x60, then either falls back to animation 2 (budget left) or
/// turns about: animation 3, state 2, a fresh budget of 1000 per unit of the
/// placement record's `variant`, and `yaw` / `targetYaw` set to the
/// current yaw and its opposite. State 2 turns at `turnRate` = 0x3B until
/// `animFrame` reaches 0x23, then returns to animation 2 and state 0. A set
/// `playerSpotted` or `gSceneCombatState.golemPawnRookDeathAlert` overrides everything with animation 2, entry 2
/// and the shared state-F0 slot.
void golemPawnRookApproachState(Task* arg0)
{
    Enemy*             spawn;
    GolemPawnRookWork* work;
    GfxCoord*          self;
    u8*                head;
    s16                state;
    s16                delta;
    s32                ang;
    s32                param;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;

    self  = arg0->extra.tmd->coords;
    work  = arg0->work;
    spawn = arg0->spawnArg2.pointer;
    state = work->step;

    switch (state) {
        case 0:
            delta = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                delta = 0x14;
            }
            work->forwardSpeed        = delta;
            work->turnRate            = 0;
            work->patrolDistanceLeft -= work->forwardSpeed;
            if (work->patrolDistanceLeft <= 0) {
                work->anim         = 4;
                work->timer        = 0;
                work->step         = 1;
                work->forwardSpeed = 0;
            }
            golemPawnRookCheckProximity(arg0);
            break;
        case 1:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->animFrame >= 0x60) {
                if (work->patrolDistanceLeft <= 0) {
                    param                    = spawn->place->variant;
                    work->anim               = 3;
                    work->step               = 2;
                    work->patrolDistanceLeft = param * 1000;
                    ang                      = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->yaw                = ang;
                    work->targetYaw          = (ang + 0x800) & 0xFFF;
                } else {
                    work->anim = 2;
                    work->step = 0;
                }
            }
            break;
        case 2:
            work->forwardSpeed = 0;
            work->turnRate     = 0x3B;
            if (work->animFrame >= 0x23) {
                work->anim = 2;
                work->step = 0;
            }
            break;
    }

    if ((work->playerSpotted != 0) || (gSceneCombatState.golemPawnRookDeathAlert != 0)) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = 0;
        work->anim     = 2;
        work->timer    = 0;
        sceneEngageBattle(1);
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
