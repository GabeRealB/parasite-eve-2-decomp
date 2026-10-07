/* Part of the stride walk library; see stride_walk.h. */

/* ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW is an object-like binding to a
 * declared static void(Task*) drawer. The call evaluates task once. */
#ifndef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW before including this fragment"
#endif

/// Ramps the head-aim weight toward the selected endpoint or back to zero.
///
/// Borrows writable `work` with weight in 0..`STRIDE_WALK_TURN_WEIGHT_FULL`.
/// Each call changes the Q12 weight by one eighth of full weight, saturating
/// at the selected endpoint. Any mode other than TURN_PLAYER releases the aim.
static __inline__ void _strideWalkStepHeadTurnWeight(StrideWalkWork* work)
{
    enum { STRIDE_WALK_TURN_WEIGHT_STEP = STRIDE_WALK_TURN_WEIGHT_FULL / 8 };

    if (work->turnMode == STRIDE_WALK_TURN_PLAYER) {
        work->turnWeight += STRIDE_WALK_TURN_WEIGHT_STEP;
        if (work->turnWeight > STRIDE_WALK_TURN_WEIGHT_FULL) {
            work->turnWeight = STRIDE_WALK_TURN_WEIGHT_FULL;
        }
    } else {
        work->turnWeight -= STRIDE_WALK_TURN_WEIGHT_STEP;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
}

/// Updates the soldier's lighting, scripted walk, player-facing head and shadow.
///
/// `task` owns the spawned TMD walker and `StrideWalkWork`; the player slot
/// must contain a live model with its root-to-head parts 0..4. The walker rig,
/// borrowed model/clip storage and scratch/GTE state must remain initialized.
/// Samples lighting 800 world units above the composed root before moving it,
/// then applies the animation update before head aim. Aim weight changes by
/// 512/4096 per call; nominal yaw/pitch limits are 512/256 angle units (4096
/// per turn), widened by the aim helper to preserve an existing extreme pose.
/// `enemy` is the unused enemy-state callback argument. No pointer is retained.
static void _strideWalkFrame(Enemy* enemy, Task* task)
{
    enum {
        STRIDE_WALK_LIGHT_SAMPLE_HEIGHT = 800,
        STRIDE_WALK_LIGHT_COUNT         = 3,
        STRIDE_WALK_HEAD_YAW_LIMIT      = 0x200,
        STRIDE_WALK_HEAD_PITCH_LIMIT    = 0x100,
    };

    StrideWalkWork* work;
    GfxCoord*       rootCoord;
    TmdObject*      model;
    VECTOR          lightSamplePosition;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = task->work;
    // Lighting samples the root before this frame's movement and pose changes.
    actorRenderComposeCoord(rootCoord);
    lightSamplePosition.vx = rootCoord->workm.t[0];
    lightSamplePosition.vy = rootCoord->workm.t[1] - STRIDE_WALK_LIGHT_SAMPLE_HEIGHT;
    lightSamplePosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightSamplePosition, 0, STRIDE_WALK_LIGHT_COUNT);
    _strideWalkUpdate(task);
    // Apply head aim to the pose produced by the animation update.
    _strideWalkStepHeadTurnWeight(work);
    animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), STRIDE_WALK_HEAD_YAW_LIMIT,
                           STRIDE_WALK_HEAD_PITCH_LIMIT, work->turnWeight);
    ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW(task);
}
