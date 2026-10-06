/* Part of the pair walk library; see pair_walk.h. */

/// Consumes one attempted walk step and records the idle choice when travel ends.
///
/// Borrows live, writable `work` with a nonzero signed-halfword `st.travel`
/// count of remaining attempts. The caller counts a step even when actor
/// freezing suppresses translation. Decrements without clamping; only a result
/// of zero selects `PAIR_WALK_ANIM_IDLE` and records `PAIR_WALK_IDLE_BLEND_FRAMES`
/// whole frames for a later blend. Leaves `st.state` and the playing tracks
/// unchanged, so this records a choice without requesting an animation reseed.
static __inline__ void _pairWalkCompleteTravelTick(PairWalkWork* work)
{
    work->st.travel--;
    if (work->st.travel == 0) {
        work->blendFrames = PAIR_WALK_IDLE_BLEND_FRAMES;
        work->st.animId   = PAIR_WALK_ANIM_IDLE;
    }
}

/// Applies animation requests or advances the walker by a 12-unit travel tick.
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
        // Keep the actor-freeze test's constant separate from the state test's.
        do {
        } while (0);
        requestedAnimId = work->st.animId;
        if (requestedAnimId == PAIR_WALK_ANIM_WALK && work->st.travel != 0) {
            _actorMovementStepModelForward(task, PAIR_WALK_MODEL_STEP_UNITS);
            // Travel counts attempted steps, including those suppressed by freezing.
            _pairWalkCompleteTravelTick(work);
        }
        _pairWalkTickAnim(task);
        return;
    }
}
