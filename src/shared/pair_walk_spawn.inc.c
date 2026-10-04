/* Part of the pair walk library; see pair_walk.h. */

/// Spawn handler of the enemy this actor's model task carries: state 0 of
/// `func_actor_450800_80133264`'s `fns` table. Builds the enemy's `PairWalkWork` block,
/// spawns its own model task out of the same `gPairWalkTasks` table,
/// faces it at the placed spawn point, starts the animation and hands the state
/// machine to `pairWalkUpdate`.
void pairWalkSpawn(Enemy* enemy, Task* task)
{
    VECTOR        vec;
    PairWalkWork* work;
    GfxCoord*     coord;
    TmdObject*    obj;
    Enemy*        spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (work = memCalloc(sizeof(PairWalkWork), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = pairWalkExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = Gp_SpawnEnemyFromTable(gPairWalkTasks, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    taskReparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)gPairWalkAnimParams, obj,
                         work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = gPairWalkMessages;
    pairWalkUpdate(task);
    task->state++;
}
