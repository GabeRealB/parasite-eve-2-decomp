/* Part of the view figure library; see view_figure.h. */

/// Step 0 of the `func_actor_110800_801322A0` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `gViewFigureWork` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
void viewFigureSpawnState(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    void*      work;
    TmdObject* obj;
    GfxCoord*  coord;

    obj             = task->extra.tmd;
    coord           = obj->coords;
    work            = memCalloc(sizeof(ViewFigureWork), 0);
    gViewFigureWork = work;
    task->work      = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _viewFigureExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    obj->otOffset                    = 0;
    coord->composeStamp              = GRAPHICS_COORD_DIRTY;
    gActorSelfTask                   = task;
    gActorHelperTask                 = taskSpawnFromTable(gViewFigureTasks, 1, 0, 0);
    animationInitContext(&gViewFigureWork->rig.anim, (AnimationSet**)gViewFigureAnimSets, obj,
                         gViewFigureWork->rig.poses, gViewFigureWork->rig.slots);
    gViewFigureWork->st.animId = 1;
    gViewFigureWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    viewFigureStepAnim(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    gViewFigureWork->st.field_6++;
    task->msgTable = gViewFigureMessages;
    task->state++;
}
