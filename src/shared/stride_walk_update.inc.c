/* Part of the stride walk library; see stride_walk.h. */

/// Consumes one attempted travel step and queues idle when the count expires.
///
/// Borrows writable `work` after a movement attempt with nonzero `st.travel`,
/// including one suppressed by actor freezing. The signed-halfword count is
/// decremented without clamping. A stored zero requests idle with a ten-frame
/// blend; the caller still ticks the old tracks before the next update reseeds.
static __inline__ void _strideWalkCompleteTravelTick(StrideWalkWork* work)
{
    enum {
        STRIDE_WALK_ANIM_IDLE         = 1,
        STRIDE_WALK_IDLE_BLEND_FRAMES = 10,
    };

    work->st.travel--;
    if (work->st.travel == 0) {
        work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
        work->blendFrames = STRIDE_WALK_IDLE_BLEND_FRAMES;
        work->st.animId   = STRIDE_WALK_ANIM_IDLE;
    }
}

/// Applies an animation request or advances one scheduled stride-walk step.
///
/// `task` owns a live TMD model and `StrideWalkWork` with its twenty-slot rig
/// bound to loaded clips and coordinates. The carrier's paced-walk bindings
/// must select this walker's tick, reset and blend instances. Borrowed model
/// and clip storage and the helpers' scratch/GTE state must remain live.
/// BLEND captures old poses and RESET restarts tracks; both enter TICK and
/// return without an ordinary slot tick. Other states besides TICK do nothing.
///
/// TICK attempts a 30-parent-coordinate-unit step along normalized local Z
/// while WALK is requested and `st.travel` is nonzero, then ticks slots 1..19.
/// Frozen actors still consume travel attempts. The final attempt queues the
/// ten-frame idle blend for the next call while still ticking the old clip.
static void _strideWalkUpdate(Task* task)
{
    enum { STRIDE_WALK_ANIM_WALK = 4 };
    StrideWalkWork* work;
    s16             requestedAnimId;

    work = task->work;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        PACED_WALK_BLEND_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        PACED_WALK_RESET_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        // The loop-end note keeps the freeze comparison's constant separate
        // from the animation-state comparison in this compiler's CSE pass.
        do {
        } while (0);
        requestedAnimId = work->st.animId;
        if (requestedAnimId == STRIDE_WALK_ANIM_WALK && work->st.travel != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, STRIDE_WALK_STEP_DISTANCE);
            // Count the attempt even when actor freezing suppressed translation.
            _strideWalkCompleteTravelTick(work);
        }
        PACED_WALK_TICK_ANIM(task);
        return;
    }
}
