/* Part of the paced walk library; see paced_walk.h. */

// A carrier can include the update for more than one paced walker.
#ifndef SRC_SHARED_PACED_WALK_COMPLETE_TRAVEL_TICK
#define SRC_SHARED_PACED_WALK_COMPLETE_TRAVEL_TICK

/// Consumes one scheduled travel attempt and records idle when the count expires.
///
/// Borrows live, writable `work` with nonzero `st.travel`, after the caller's
/// movement attempt, including one suppressed by actor freezing. The count
/// stays signed 16-bit: negative values also decrement, wrapping at -32768
/// rather than clamping. Expiration measures attempts, not physical arrival.
///
/// A stored result of zero selects clip 1 and ten whole normal-rate frames
/// for a later blend. Playback and `st.state` are unchanged; a later reseed
/// must apply the selection. No pointer is retained.
static __inline__ void _pacedWalkCompleteTravelTick(PacedWalkWork* work)
{
    enum {
        PACED_WALK_ANIM_IDLE         = 1,
        PACED_WALK_IDLE_BLEND_FRAMES = 10,
    };

    work->st.travel--;
    if (work->st.travel == 0) {
        work->blendFrames = PACED_WALK_IDLE_BLEND_FRAMES;
        work->st.animId   = PACED_WALK_ANIM_IDLE;
    }
}
#endif

/// Processes a paced walker's animation request or one attempted travel step.
///
/// `task` must own a live TMD model and `PacedWalkWork` with its twenty-slot
/// rig bound to model coordinates and loaded clips. The selected animation
/// helpers must use the same task and work block. Keep borrowed model and clip
/// storage live, with the movement and animation helpers' scratch/GTE state
/// initialized. BLEND captures the old poses and RESET restarts the requested
/// tracks; each enters TICK and returns without an ordinary animation tick.
/// Other states besides TICK do nothing. Request states, clip-bank indices,
/// track bounds and blend durations are not validated here.
///
/// TICK attempts a 12-parent-coordinate-unit step along the normalized local
/// Z axis only for requested walk clip 4 with nonzero `st.travel`, then ticks
/// non-root slots 1 through 19. Travel counts attempted calls, even when actor
/// freezing suppresses movement; the signed-halfword count is not clamped.
/// At zero it records idle clip 1 and a ten-frame blend duration, leaving TICK
/// and the playing tracks intact. A later reseed request applies a clip change.
static void PACED_WALK_UPDATE(Task* task)
{
    enum {
        PACED_WALK_ANIM_WALK  = 4,
        PACED_WALK_STEP_UNITS = 12,
    };

    PacedWalkWork* work;

    work = task->work;
    // Reseeding installs the request; ordinary ticking resumes on the next call.
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
        // The loop-end note ends cse's first block here, so the actor-freeze check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        if (work->st.animId == PACED_WALK_ANIM_WALK && work->st.travel != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, PACED_WALK_STEP_UNITS);
            // Frozen actors still consume the scheduled travel attempt.
            _pacedWalkCompleteTravelTick(work);
        }
        PACED_WALK_TICK_ANIM(task);
        return;
    }
}
