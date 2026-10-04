/* Part of the footstep walk library; see footstep_walk.h. */

/// Per-frame update: states 1 and 2 run their one-shot animation restart and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `gFootstepWalkMode`) and, when the
/// walk ends, queues clip 0xD through state 1; it turns the model while
/// `turnFrames` counts down in clip 3, then ticks the animation and, once
/// `playFootsteps` is set, plays the footsteps.
void footstepWalkUpdate(Task* task)
{
    GfxCoord*         coord = task->extra.tmd->coords;
    FootstepWalkWork* work  = task->work;

    if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        footstepWalkBlendAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        footstepWalkResetAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (gFootstepWalkMode) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state           = ACTOR_ENEMY_ANIM_BLEND;
                    gFootstepWalkBlendFrames = 10;
                    work->st.animId          = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        footstepWalkTickAnim();
        if (work->playFootsteps != 0) {
            footstepWalkPlaySteps(task);
        }
    }
}
