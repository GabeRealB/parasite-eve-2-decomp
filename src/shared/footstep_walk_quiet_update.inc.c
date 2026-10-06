/* Part of the footstep walk library; see footstep_walk.h. */

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `st.travel` counts down, by a distance the
/// approach mode in `gFootstepWalkMode` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `turnFrames` counts down. Mode 3 then ticks the animation.
void footstepWalkQuietUpdate(Task* task)
{
    GfxCoord*              coord = task->extra.tmd->coords;
    FootstepWalkQuietWork* work  = task->work;

    if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        footstepWalkQuietBlendAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        footstepWalkQuietResetAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (gFootstepWalkMode) {
                    case 0:
                        _actorMovementStepModelForward(task, 0x3C);
                        break;
                    case 1:
                        _actorMovementStepModelForward(task, -0xF);
                        break;
                    case 2:
                        _actorMovementStepModelForward(task, 0x19);
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
    }
}
