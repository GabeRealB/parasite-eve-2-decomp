/* Part of the footstep walk library; see footstep_walk.h. */

/// Binds the published rig and queues the startup clip with movement and sound off.
///
/// `model` and `gFootstepWalkWork` must be live, with the carrier's clip table
/// loaded. The context borrows the model, table and rig storage. This prepares
/// a reset request; the caller must process it before ticking any slot.
static __inline__ void _footstepWalkPrepareAnimation(TmdObject* model)
{
    enum { FOOTSTEP_WALK_STARTUP_ANIM_ID = 1 };

    animationInitContext(&gFootstepWalkWork->rig.anim, (AnimationSet**)gFootstepWalkAnims, model,
                         gFootstepWalkWork->rig.poses, gFootstepWalkWork->rig.slots);
    gFootstepWalkWork->st.animId     = FOOTSTEP_WALK_STARTUP_ANIM_ID;
    gFootstepWalkWork->st.state      = ACTOR_ENEMY_ANIM_RESET;
    gFootstepWalkWork->st.travel     = 0;
    gFootstepWalkWork->turnFrames    = 0;
    gFootstepWalkWork->stepRecord    = NULL;
    gFootstepWalkWork->playFootsteps = false;
}

/// Initializes the sound-enabled NPC walker's model and playback in task state 0.
///
/// `enemy` is the live object in `task->spawnArg2.pointer`; `task` must own a
/// nineteen-part TMD model and have no work allocation yet. The primary heap
/// and lighting query's view, scratch and GTE state must be ready. The carrier's
/// native clip table must have clip 1 loaded with tracks 1 through 18, and its
/// message table and clip data must remain loaded while the task lives.
///
/// The task owns the zeroed `FootstepWalkWork`; the model borrows its matrices
/// and the animation context borrows the rig and model coordinates. Publishes
/// the work and task for singleton message handlers, starts clip 1 without a
/// pose tick, and enters task state 1 with travel, turning and footsteps off.
/// The exit callback releases the enemy and starts task/work/model teardown;
/// the published pointers must not be used afterwards. Allocation failure
/// destroys the enemy and starts task teardown immediately; either argument
/// may be invalid on return.
static void _footstepWalkSpawn(Enemy* enemy, Task* task)
{
    enum {
        FOOTSTEP_WALK_OT_ENTRY_OFFSET       = 1,
        FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET = -800
    };

    VECTOR3           lightSamplePosition;
    FootstepWalkWork* allocatedWork;
    TmdObject*        model;
    GfxCoord*         rootCoord;

    model             = task->extra.tmd;
    rootCoord         = model->coords;
    allocatedWork     = memCalloc(sizeof(*allocatedWork), false);
    gFootstepWalkWork = allocatedWork;
    task->work        = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    // The task owns the block; the model borrows its lighting matrices.
    task->exitCallback               = _footstepWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = FOOTSTEP_WALK_OT_ENTRY_OFFSET;
    model->lightMtx                  = &gFootstepWalkWork->light;
    model->colorMtx                  = &gFootstepWalkWork->color;

    // Sample all light rows at the existing cached root position, offset in Y.
    lightSamplePosition.vx = rootCoord->workm.t[0];
    lightSamplePosition.vy = rootCoord->workm.t[1] + FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET;
    gFootstepWalkTask      = task;
    lightSamplePosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightSamplePosition, 0, ARRAY_SIZE(model->colorMtx->m[0]));

    // Bind borrowed playback storage, then reset the driven slots without ticking.
    _footstepWalkPrepareAnimation(model);
    task->msgTable = gFootstepWalkMsgTable;
    _footstepWalkUpdate(task);
    task->state += 1;
}
