/* Part of the stride walk library; see stride_walk.h. */

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (blending over `blendFrames` frames, or from the clip's
/// start) and advance to state 3; state 3 walks the root coordinate 0x1E units
/// per frame while clip 4 has `travel` left, requesting clip 1 with a 10-frame
/// blend when it runs out, then ticks the slots.
void strideWalkUpdate(Task* task)
{
    StrideWalkWork* work;
    s16             animId;

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
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, 0x1E);
            work->st.travel--;
            if (work->st.travel == 0) {
                work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
                work->blendFrames = 0xA;
                work->st.animId   = 1;
            }
        }
        PACED_WALK_TICK_ANIM(task);
        return;
    }
}
