/* Part of the stride walk library; see stride_walk.h. */

/// Binds work-owned matrices and samples cached root XYZ with Y minus 800.
///
/// Does not compose the root; the model borrows matrices through task teardown.
/// Requires live model/work/root and initialized room-light and scratch/GTE state.
static inline void _strideWalkInitializeModelLighting(TmdObject* model, StrideWalkWork* work, const GfxCoord* rootCoord)
{
    enum { STRIDE_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET = 800,
           STRIDE_WALK_LIGHT_COUNT                 = 3 };
    VECTOR lightingSample;
    model->lightMtx   = &work->light;
    model->colorMtx   = &work->color;
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - STRIDE_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, STRIDE_WALK_LIGHT_COUNT);
}

/// Initializes a scripted stride walker with an optional carried rifle model.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Parents
/// the untargetable root to the view. Nonzero spawnArg1 creates descriptor slot
/// 1 as a live child and requests armed clip 2; zero requests unarmed clip 1.
/// Resets head aim, binds work-owned matrices and samples cached XYZ with Y
/// minus 800 before composition. Initializes the borrowed 20-slot clip bank,
/// installs messages/teardown, updates once and advances task state 0 to 1.
/// Requires live enemy/model and initialized view, room-light and scratch/GTE
/// state; requested child creation must succeed and clips outlive the rig.
static void _strideWalkSpawn(Enemy* enemy, Task* task)
{
    enum {
        STRIDE_WALK_ANIM_UNARMED_IDLE = 1,
        STRIDE_WALK_ANIM_ARMED_IDLE   = 2
    };
    StrideWalkWork* work;
    GfxCoord*       rootCoord;
    TmdObject*      model;
    Enemy*          pairedEnemy;

    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    work       = memCalloc(sizeof(StrideWalkWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _strideWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    if (task->spawnArg1.value != 0) {
        pairedEnemy = enemySpawnFromTable(gStrideWalkTasks, 1, 0, enemy);
        taskReparent(task, pairedEnemy->task);
        work->pairTask  = pairedEnemy->task;
        work->st.animId = STRIDE_WALK_ANIM_ARMED_IDLE;
    } else {
        work->st.animId = STRIDE_WALK_ANIM_UNARMED_IDLE;
    }
    work->turnMode   = STRIDE_WALK_TURN_RELEASE;
    work->turnWeight = 0;
    _strideWalkInitializeModelLighting(model, work, rootCoord);
    animationInitContext(&work->rig.anim, (AnimationSet**)gStrideWalkAnimParams, model,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = gStrideWalkMessages;
    _strideWalkUpdate(task);
    task->state += 1;
}
