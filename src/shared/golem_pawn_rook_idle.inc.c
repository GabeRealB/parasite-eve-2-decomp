/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0 of the `behavior` state table, the idle. State 0 counts
/// `timer` up to 0x5B frames and then switches to animation 4 and state 1,
/// running the proximity check `golemPawnRookCheckProximity` every frame meanwhile;
/// state 1 waits for `animFrame` to reach 0x5E and drops back to state 0 with
/// animation 1. A set `playerSpotted` or `gSceneCombatState.golemPawnRookDeathAlert` overrides both with
/// animation 2, entry 2 and the shared state-F0 slot.
void golemPawnRookIdleState(Task* arg0)
{
    GolemPawnRookWork* work;
    s16                state;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            work->timer++;
            if (work->timer >= 0x5B) {
                work->anim  = 4;
                work->timer = 0;
                work->step  = 1;
            }
            golemPawnRookCheckProximity(arg0);
            break;
        case 1:
            if (work->animFrame >= 0x5E) {
                work->anim = 1;
                work->step = 0;
            }
            break;
    }

    if ((work->playerSpotted != 0) || (gSceneCombatState.golemPawnRookDeathAlert != 0)) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = 0;
        work->anim     = 2;
        work->timer    = 0;
        Gp_ArmStateF0(1);
    }
}
