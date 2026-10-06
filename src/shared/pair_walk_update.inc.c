/* Part of the pair walk library; see pair_walk.h. */

/// Consumes an attempted movement tick and records the idle choice on arrival.
static __inline__ void _pairWalkCompleteTravelTick(PairWalkWork* work)
{
    work->st.travel--;
    if (work->st.travel == 0) {
        work->blendFrames = PAIR_WALK_IDLE_BLEND_FRAMES;
        work->st.animId   = PAIR_WALK_ANIM_IDLE;
    }
}

/// Applies animation requests or advances the walker by a 17-unit travel tick.
///
/// Requires a live TMD walker with `PairWalkWork` and animation's loaded data,
/// initialized scratch stack and GTE. BLEND captures poses and RESET restarts
/// tracks, then each enters TICK and returns without another slot tick. TICK
/// moves along local Z only while the requested clip is WALK and travel is
/// nonzero, then advances slots 1 to 18. Other states do nothing.
/// Travel counts down even when actor freezing suppresses translation. Arrival
/// records IDLE and a ten-frame blend duration but leaves TICK and the playing
/// tracks unchanged; another request is needed to apply a clip change.
static void _pairWalkUpdate(Task* task)
{
    enum { PAIR_WALK_STEP_UNITS = 17 };
    PairWalkWork* work;
    s16           requestedAnimId;

    work = task->work;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _pairWalkReseedAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _pairWalkResetAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        // The loop-end note ends cse's first block here, so the actor-freeze check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        requestedAnimId = work->st.animId;
        if (requestedAnimId == PAIR_WALK_ANIM_WALK && work->st.travel != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, PAIR_WALK_STEP_UNITS);
            // Travel counts attempted steps, including those suppressed by freezing.
            _pairWalkCompleteTravelTick(work);
        }
        _pairWalkTickAnim(task);
        return;
    }
}
