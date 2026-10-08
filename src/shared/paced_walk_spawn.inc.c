/* Part of the paced walk library; see paced_walk.h. */

/// Binds work-owned matrices and samples cached root XYZ with Y minus 800.
///
/// Does not compose the root; the model borrows matrices through task teardown.
/// Requires live model/work/root and initialized room-light and scratch/GTE state.
static inline void _pacedWalkInitializeModelLighting(TmdObject* model, PacedWalkWork* work, const GfxCoord* rootCoord)
{
    enum { PACED_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET = 800,
           PACED_WALK_LIGHT_COUNT                 = 3 };
    VECTOR lightingSample;
    model->lightMtx   = &work->light;
    model->colorMtx   = &work->color;
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - PACED_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, PACED_WALK_LIGHT_COUNT);
}

/// Initializes a scripted paced walker and runs its first motion/animation update.
///
/// Allocates task-owned zeroed work, destroying the enemy on failure. Parents
/// the model root to the view, disables lock-on, binds work-owned light/color
/// matrices and samples cached XYZ with Y minus 800 before any composition.
/// Initializes the 20-slot rig from the carrier's borrowed clip bank, requests
/// clip 10, installs messages/teardown and advances task state 0 to frame state 1.
/// Requires live enemy/model, resident view and initialized room/scratch/GTE
/// state; model and clip storage must outlive the task's animation context.
static void _pacedWalkSpawn(Enemy* enemy, Task* task)
{
    enum {
        PACED_WALK_ANIM_SPAWN = 10
    };
    PacedWalkWork* work;
    PacedWalkWork* allocatedWork;
    GfxCoord*      rootCoord;
    TmdObject*     model;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memCalloc(sizeof(PacedWalkWork), false);
    work          = allocatedWork;
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pacedWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    model->flags                     = 0;
    work->st.animId                  = PACED_WALK_ANIM_SPAWN;
    work->enemy                      = enemy;
    _pacedWalkInitializeModelLighting(model, work, rootCoord);
    animationInitContext(&work->rig.anim, (AnimationSet**)gPacedWalkAnimBank, model,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = gPacedWalkMsgTable;
    PACED_WALK_UPDATE(task);
    task->state++;
}
