/* Part of the footstep walk library; see footstep_walk.h. */

/// Advances a quiet walker's model yaw and consumes one scheduled turn update.
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
static __inline__ void _footstepWalkQuietTurnModel(GfxCoord* rootCoord, FootstepWalkQuietWork* work)
{
    work->st.yaw += FOOTSTEP_WALK_TURN_ANGLE_PER_UPDATE;
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->turnFrames--;
}

/// Processes an animation restart or one moving and turning update of a quiet walker.
///
/// `task->work` and `_gFootstepWalkWork` must name the same live quiet-walker
/// block with its rig bound to loaded clips and a live model. Scratch/GTE and
/// borrowed-storage requirements are those of the movement and animation
/// helpers. Restart requests enter tick state without running its ordinary
/// tick branch; the blend path still advances slots to capture their old poses.
///
/// Walk clips consume travel frames at the independently selected mode's
/// distance, including when actor freezing suppresses translation. The last
/// travel call queues a ten-frame idle blend for the next update but still
/// ticks the old clip. Turn-clip calls add 51/4096 turns and consume one
/// `turnFrames` count, retaining the heading's low 16 bits. Tick state always
/// advances slots afterward; no sound state is accessed. Other states do nothing.
static void _footstepWalkQuietUpdate(Task* task)
{
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    FootstepWalkQuietWork* work      = task->work;

    // Restart requests consume this call; root movement starts in tick state.
    if (_gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _footstepWalkQuietBlendAnim();
        _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (_gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _footstepWalkQuietResetAnim();
        _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (_gFootstepWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == FOOTSTEP_WALK_ANIM_WALK_14 || work->st.animId == FOOTSTEP_WALK_ANIM_WALK_2 ||
            work->st.animId == FOOTSTEP_WALK_ANIM_WALK_15) {
            if (work->st.travel != 0) {
                switch (_gFootstepWalkMode) {
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
                    work->st.state            = ACTOR_ENEMY_ANIM_BLEND;
                    _gFootstepWalkBlendFrames = FOOTSTEP_WALK_IDLE_BLEND_FRAMES;
                    work->st.animId           = FOOTSTEP_WALK_ANIM_IDLE;
                }
            }
        }
        if (work->st.animId == FOOTSTEP_WALK_ANIM_TURN && work->turnFrames != 0) {
            _footstepWalkQuietTurnModel(rootCoord, work);
        }
        _footstepWalkTickAnim();
    }
}
