/* Part of the scripted walk library; see scripted_walk.h. */

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `st.travel` counts down, by a distance the
/// approach mode in `SCRIPTED_WALK_MODE` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `turnFrames` counts down. Mode 3 then ticks the animation.
void scriptedWalkUpdate(Task* task)
{
    GfxCoord*         coord = task->extra.tmd->coords;
    ScriptedWalkWork* work  = (ScriptedWalkWork*)task->work;

    if (gScriptedWalkWork->st.state == 1) {
        scriptedWalkBlendAnim();
        gScriptedWalkWork->st.state = 3;
    } else if (gScriptedWalkWork->st.state == 2) {
        scriptedWalkResetAnim();
        gScriptedWalkWork->st.state = 3;
    } else if (gScriptedWalkWork->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (SCRIPTED_WALK_MODE) {
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
                    work->st.state           = 1;
                    gScriptedWalkBlendFrames = 10;
                    work->st.animId          = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        scriptedWalkTickAnim();
    }
}
