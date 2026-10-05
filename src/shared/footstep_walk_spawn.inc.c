/* Part of the footstep walk library; see footstep_walk.h. */

/// State 0 of the enemy's task: allocates the work block, publishes it in
/// `gFootstepWalkWork` and on the task's work slot, points the model's
/// light and colour matrices and its animation context at it, publishes the
/// task in `gFootstepWalkTask`, then runs the runner once and advances
/// the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
void footstepWalkSpawn(Enemy* enemy, Task* task)
{
    VECTOR            vec;
    FootstepWalkWork* work;
    TmdObject*        obj;
    GfxCoord*         coord;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(sizeof(FootstepWalkWork), 0);
    gFootstepWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = footstepWalkExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->lightMtx                    = &gFootstepWalkWork->light;
    obj->colorMtx                    = &gFootstepWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    gFootstepWalkTask                = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&gFootstepWalkWork->rig.anim, (AnimationSet**)gFootstepWalkAnims, obj,
                         gFootstepWalkWork->rig.poses, gFootstepWalkWork->rig.slots);
    gFootstepWalkWork->st.animId     = 1;
    gFootstepWalkWork->st.state      = ACTOR_ENEMY_ANIM_RESET;
    gFootstepWalkWork->st.travel     = 0;
    gFootstepWalkWork->turnFrames    = 0;
    gFootstepWalkWork->stepRecord    = NULL;
    gFootstepWalkWork->playFootsteps = 0;
    task->msgTable                   = gFootstepWalkMsgTable;
    footstepWalkUpdate(task);
    task->state += 1;
}
