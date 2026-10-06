/* Part of the footstep walk library; see footstep_walk.h. */

/// Advances a footstep walker's model yaw and consumes one scheduled turn update.
///
/// `rootCoord` is the live model root belonging to the writable `work` block.
/// The caller selects the turn clip and checks `work->turnFrames` before calling;
/// scheduled counts run from 20 down to zero, one call per turning update.
/// Adds 51 angle units (4096 per turn) to `work->st.yaw`, narrowing to its signed
/// 16-bit storage before applying the heading in the root's parent space.
/// Replaces the rotation with a unit-scale yaw, preserving translation, and
/// marks the cached composition stale; composition is left to the renderer.
///
/// Both arguments are borrowed for this call. The rotation requires an initialized
/// scratch stack with 0x24 free, word-aligned bytes disjoint from both objects;
/// that reservation is released before return.
static __inline__ void _footstepWalkTurnModel(GfxCoord* rootCoord, FootstepWalkWork* work)
{
    work->st.yaw += FOOTSTEP_WALK_TURN_ANGLE_PER_UPDATE;
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->turnFrames--;
}

/// Processes an animation restart or one moving, turning and footstep update.
///
/// `task` must own the live sound walker and its bound rig, with
/// `gFootstepWalkWork` naming the same work block. Borrowed model/clip storage
/// and the movement, animation and sound helpers' scratch/GTE state must be
/// live. Blend and reset requests enter tick state without running the ordinary
/// tick branch; blending still advances each slot to capture its previous pose.
///
/// In tick state, walk clips consume `st.travel` at the mode's distance per
/// call, even if actor freezing suppresses translation. The final travel call
/// queues the idle blend for the next update and still ticks the old clip.
/// Turn-clip updates add 51/4096 turns while `turnFrames` is nonzero, retaining
/// the heading's low 16 bits. Foot cues are tested after the slot tick once
/// `playFootsteps` is enabled. Other animation states do nothing.
static void _footstepWalkUpdate(Task* task)
{
    GfxCoord*         rootCoord = task->extra.tmd->coords;
    FootstepWalkWork* work      = task->work;

    // Restart requests consume this call; root movement starts in tick state.
    if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _footstepWalkBlendAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _footstepWalkResetAnim();
        gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == FOOTSTEP_WALK_ANIM_WALK_14 || work->st.animId == FOOTSTEP_WALK_ANIM_WALK_2 ||
            work->st.animId == FOOTSTEP_WALK_ANIM_WALK_15) {
            if (work->st.travel != 0) {
                switch (gFootstepWalkMode) {
                    case FOOTSTEP_WALK_MODE_FORWARD:
                        _actorMovementStepModelForward(task, FOOTSTEP_WALK_FORWARD_DISTANCE);
                        break;
                    case FOOTSTEP_WALK_MODE_BACKWARD:
                        _actorMovementStepModelForward(task, -FOOTSTEP_WALK_BACKWARD_DISTANCE);
                        break;
                    case FOOTSTEP_WALK_MODE_SLOW_FORWARD:
                        _actorMovementStepModelForward(task, FOOTSTEP_WALK_SLOW_FORWARD_DISTANCE);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state           = ACTOR_ENEMY_ANIM_BLEND;
                    gFootstepWalkBlendFrames = FOOTSTEP_WALK_IDLE_BLEND_FRAMES;
                    work->st.animId          = FOOTSTEP_WALK_ANIM_IDLE;
                }
            }
        }
        if (work->st.animId == FOOTSTEP_WALK_ANIM_TURN && work->turnFrames != 0) {
            _footstepWalkTurnModel(rootCoord, work);
        }
        // Sound uses slot 1's record selected by this tick and its cached view transform.
        _footstepWalkTickAnim();
        if (work->playFootsteps != 0) {
            _footstepWalkPlayStepSound(task);
        }
    }
}
