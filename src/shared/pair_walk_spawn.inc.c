/* Part of the pair walk library; see pair_walk.h. */

/// Initializes the pair walker's matrices and cached three-light sample.
///
/// Use only as standalone statements in a compound scope. Arguments must be
/// side-effect-free object/pointer lvalues: model, work and root are read
/// repeatedly, and sample is a caller-owned VECTOR lvalue. XYZ
/// retains the root cache's frame, with 800 units subtracted from Y; no compose.
/// The model borrows work matrices until teardown. Captures only the lighting
/// constants declared in the spawn function; no argument pointer is retained.
#define PAIR_WALK_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, sample)              \
    (model)->lightMtx = &(work)->light;                                                  \
    (model)->colorMtx = &(work)->color;                                                  \
    (sample).vx       = (rootCoord)->workm.t[0];                                         \
    (sample).vy       = (rootCoord)->workm.t[1] - PAIR_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET; \
    (sample).vz       = (rootCoord)->workm.t[2];                                         \
    worldCoordSetModelLighting((model), &(sample), 0, PAIR_WALK_LIGHT_COUNT)

/// Initializes a scripted pair walker and creates its carried model child.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// descriptor slot 1 to spawn a live model enemy: texture offsets come from
/// the owner's placement, and reparenting transfers that task beneath the walker.
/// The untargetable root is parented to the view; work-owned lighting matrices
/// use cached XYZ with Y minus 800 before composition. Initializes the 19-slot
/// rig with borrowed clips, requests idle clip 1, installs messages/teardown,
/// updates once and advances task state 0 to 1. Enemy, placement, model and clip
/// bank must remain live as required by their borrowed animation/texture data.
static void _pairWalkSpawn(Enemy* enemy, Task* task)
{
    enum { PAIR_WALK_LIGHT_COUNT                 = 3,
           PAIR_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET = 800 };
    VECTOR        lightingSample;
    PairWalkWork* work;
    GfxCoord*     rootCoord;
    TmdObject*    model;
    Enemy*        pairedEnemy;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (work = memCalloc(sizeof(PairWalkWork), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pairWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    pairedEnemy                      = enemySpawnFromTable(gPairWalkTasks, 1, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(pairedEnemy->task->extra.tmd, enemy);
    taskReparent(task, pairedEnemy->task);
    work->pairTask = pairedEnemy->task;
    PAIR_WALK_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, lightingSample);
    animationInitContext(&work->rig.anim, (AnimationSet**)gPairWalkAnimParams, model,
                         work->rig.poses, work->rig.slots);
    work->st.animId = PAIR_WALK_ANIM_IDLE;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = gPairWalkMessages;
    _pairWalkUpdate(task);
    task->state++;
}

#undef PAIR_WALK_INITIALIZE_MODEL_LIGHTING
