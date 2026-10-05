/* Part of the paced walk library; see paced_walk.h. */

/// The actor's spawn routine (task state 0): allocates the work block,
/// destroying the enemy if that fails, and installs the exit callback. It then
/// lights the model at its root translation raised by 800, sets up the
/// animation context and the task's message table, and runs the step body
/// once with the plain reseed of clip 10 queued.
void pacedWalkSpawn(Enemy* enemy, Task* task)
{
    VECTOR         vec;
    PacedWalkWork* work;
    PacedWalkWork* mem;
    GfxCoord*      coord;
    TmdObject*     obj;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(PacedWalkWork), false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = pacedWalkExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    work->st.animId                  = 10;
    work->enemy                      = enemy;
    obj->lightMtx                    = &work->light;
    obj->colorMtx                    = &work->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)gPacedWalkAnimBank, obj,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = gPacedWalkMsgTable;
    pacedWalkUpdate(task);
    task->state++;
}
