/* Part of the stride walk library; see stride_walk.h. */

/// Spawn state: allocates the `StrideWalkWork` block (destroying the enemy on
/// failure), parents the root to the view, makes it untargetable, and when
/// spawnArg1 is set spawns the carried model from gStrideWalkTasks and starts
/// clip 2 (else clip 1). Lights the model from 0x320 above its root, builds the
/// rig, installs gStrideWalkMessages and runs the first update.
void strideWalkSpawn(Enemy* enemy, Task* task)
{
    VECTOR          vec;
    StrideWalkWork* work;
    GfxCoord*       coord;
    TmdObject*      obj;
    Enemy*          spawned;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = memCalloc(sizeof(StrideWalkWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = strideWalkExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    if (task->spawnArg1.value != 0) {
        spawned = enemySpawnFromTable(gStrideWalkTasks, 1, 0, enemy);
        taskReparent(task, spawned->task);
        work->pairTask  = spawned->task;
        work->st.animId = 2;
    } else {
        work->st.animId = 1;
    }
    work->turnMode   = STRIDE_WALK_TURN_RELEASE;
    work->turnWeight = 0;
    obj->lightMtx    = &work->light;
    obj->colorMtx    = &work->color;
    vec.vx           = coord->workm.t[0];
    vec.vy           = coord->workm.t[1] - 0x320;
    vec.vz           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)gStrideWalkAnimParams, obj,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = gStrideWalkMessages;
    strideWalkUpdate(task);
    task->state += 1;
}
