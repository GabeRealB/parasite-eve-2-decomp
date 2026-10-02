/* Part of the stride walk library; see stride_walk.h. */

/// Spawn state: allocates the 0x4FC-byte work block (destroying the enemy on
/// failure), parents the root to the view, makes it untargetable, and when
/// spawnArg1 is set spawns the carried model from gStrideWalkTasks and starts
/// clip 2 (else clip 1). Lights the model from 0x320 above its root, builds the
/// rig, installs gStrideWalkMessages and runs the first update.
void strideWalkSpawn(Enemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor161500Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    Enemy*           spawned;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = memCalloc(0x4FC, false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
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
        spawned = Gp_SpawnEnemyFromTable(gStrideWalkTasks, 1, 0, enemy);
        taskReparent(task, spawned->task);
        work->pairTask  = spawned->task;
        work->st.animId = 2;
    } else {
        work->st.animId = 1;
    }
    work->turnUp     = 0;
    work->turnWeight = 0;
    obj->lightMtx    = &work->light;
    obj->colorMtx    = &work->color;
    vec.vx           = coord->workm.t[0];
    vec.vy           = coord->workm.t[1] - 0x320;
    vec.vz           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)gStrideWalkAnimParams, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = gStrideWalkMessages;
    strideWalkUpdate(task);
    task->state += 1;
}
