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
