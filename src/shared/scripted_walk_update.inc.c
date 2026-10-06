/* Part of the scripted walk library; see scripted_walk.h. */

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `st.travel` counts down, by a distance the
/// approach mode in `SCRIPTED_WALK_MODE` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `turnFrames` counts down. Mode 3 then ticks the animation.
void scriptedWalkUpdate(Task* task)
{
    GfxCoord*             coord = task->extra.tmd->coords;
    SCRIPTED_WALK_WORK_T* work  = task->work;

    if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        scriptedWalkBlendAnim();
        SCRIPTED_WALK_WORK->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_RESET) {
        scriptedWalkResetAnim();
        SCRIPTED_WALK_WORK->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (SCRIPTED_WALK_MODE) {
                    case SCRIPTED_WALK_MODE_FORWARD:
                        _actorMovementStepModelForward(task, 0x3C);
                        break;
                    case SCRIPTED_WALK_MODE_BACKWARD:
                        _actorMovementStepModelForward(task, -0xF);
                        break;
                    case SCRIPTED_WALK_MODE_FORWARD_SHORT:
                        _actorMovementStepModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state           = ACTOR_ENEMY_ANIM_BLEND;
                    gScriptedWalkBlendFrames = 10;
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
        SCRIPTED_WALK_TICK_ANIM();
    }
}
