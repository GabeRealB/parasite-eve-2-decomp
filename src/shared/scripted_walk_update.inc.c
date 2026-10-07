/* Part of the scripted walk library; see scripted_walk.h. */

/// Processes an animation restart or one counted movement and animation update.
///
/// `task` must own a live TMD root and the block selected by
/// `SCRIPTED_WALK_WORK_T`; `SCRIPTED_WALK_WORK` must publish that same block.
/// All helper bindings select its initialized rig and loaded, non-NULL clips.
/// Borrowed task, model and clip storage must remain live, with the scratch/GTE
/// state required by the movement and animation helpers.
///
/// Blend/reset requests enter tick state without running the ordinary tick
/// branch; blending still advances slots to capture their previous poses.
/// Tick state consumes travel on clips 2, 14 and 15, even if freezing suppresses
/// translation or the mode is unknown. The last travel call queues clip 13's
/// ten-frame blend for the next update and still ticks the old clip. The caller
/// must provide clip 13 if travel can finish. Clip 3 consumes scheduled turns
/// independently of freezing, adding 51/4096 turns with signed-halfword
/// narrowing. Both countdowns test for nonzero and decrement with signed-halfword
/// narrowing. Other request states do nothing. No pointer is retained.
static void SCRIPTED_WALK_UPDATE(Task* task)
{
    enum {
        SCRIPTED_WALK_ANIM_WALK_2           = 2,
        SCRIPTED_WALK_ANIM_TURN             = 3,
        SCRIPTED_WALK_ANIM_IDLE             = 13,
        SCRIPTED_WALK_ANIM_WALK_14          = 14,
        SCRIPTED_WALK_ANIM_WALK_15          = 15,
        SCRIPTED_WALK_TURN_ANGLE_PER_UPDATE = 51,
    };
    GfxCoord*             rootCoord = task->extra.tmd->coords;
    SCRIPTED_WALK_WORK_T* work      = task->work;

    /// Applies one scheduled parent-space yaw step and consumes its pending count.
    ///
    /// Both arguments must be side-effect-free pointers to this walker's writable
    /// root and work block; each is evaluated repeatedly. The caller checks the
    /// turn clip and nonzero count. The heading narrows to its signed halfword
    /// before replacing the root rotation at unit scale; translation is retained.
    /// Uses this function's `SCRIPTED_WALK_TURN_ANGLE_PER_UPDATE` (4096 units per
    /// turn) and borrows 0x24 aligned scratch-stack bytes through `gfxRotMatrixY`.
    /// Defined only for this update body and undefined after the call site.
#define SCRIPTED_WALK_TURN_MODEL(modelRoot, walkerWork)                                      \
    do {                                                                                     \
        (walkerWork)->st.yaw += SCRIPTED_WALK_TURN_ANGLE_PER_UPDATE;                         \
        gfxRotMatrixY(&(modelRoot)->coord, (walkerWork)->st.yaw, GRAPHICS_ROTATION_REPLACE); \
        (modelRoot)->composeStamp = GRAPHICS_COORD_DIRTY;                                    \
        (walkerWork)->turnFrames--;                                                          \
    } while (0)

    // Restart requests consume this call; movement begins in tick state.
    if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        SCRIPTED_WALK_BLEND_ANIM();
        SCRIPTED_WALK_WORK->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_RESET) {
        SCRIPTED_WALK_RESET_ANIM();
        SCRIPTED_WALK_WORK->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (SCRIPTED_WALK_WORK->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == SCRIPTED_WALK_ANIM_WALK_14 || work->st.animId == SCRIPTED_WALK_ANIM_WALK_2 ||
            work->st.animId == SCRIPTED_WALK_ANIM_WALK_15) {
            if (work->st.travel != 0) {
                switch (SCRIPTED_WALK_MODE) {
                    case SCRIPTED_WALK_MODE_FORWARD:
                        _actorMovementStepModelForward(task, SCRIPTED_WALK_FORWARD_DISTANCE);
                        break;
                    case SCRIPTED_WALK_MODE_BACKWARD:
                        _actorMovementStepModelForward(task, -SCRIPTED_WALK_BACKWARD_DISTANCE);
                        break;
                    case SCRIPTED_WALK_MODE_FORWARD_SHORT:
                        _actorMovementStepModelForward(task, SCRIPTED_WALK_SHORT_FORWARD_DISTANCE);
                        break;
                }
                if (--work->st.travel == 0) {
                    // Defer the idle reseed; this call still advances the old tracks.
                    work->st.state             = ACTOR_ENEMY_ANIM_BLEND;
                    SCRIPTED_WALK_BLEND_FRAMES = SCRIPTED_WALK_IDLE_BLEND_FRAMES;
                    work->st.animId            = SCRIPTED_WALK_ANIM_IDLE;
                }
            }
        }
        if (work->st.animId == SCRIPTED_WALK_ANIM_TURN && work->turnFrames != 0) {
            SCRIPTED_WALK_TURN_MODEL(rootCoord, work);
        }
#undef SCRIPTED_WALK_TURN_MODEL
        SCRIPTED_WALK_TICK_ANIM();
    }
}
