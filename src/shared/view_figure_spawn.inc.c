/* Part of the view figure library; see view_figure.h. */

/// Initializes the singleton scripted figure and its carrier's helper model task.
///
/// Publishes newly allocated zeroed work through both the task and the carrier's
/// global pointer; allocation failure destroys the enemy. Parents the untargetable
/// model to the view, publishes the owner/helper task pointers, initializes its
/// borrowed 20-slot clip bank and requests idle clip 1. The initial animation
/// update precedes the cached-root three-light query and message installation.
/// Success advances task state 0 to 1. Requires live model/enemy, selected carrier
/// tables and initialized room/scratch/GTE state; the singleton is replaced on
/// each spawn, so callers must use only its current live instance. Teardown owns
/// work/helper lifetime; the borrowed clip bank must remain live through it.
static void _viewFigureSpawnState(Enemy* enemy, Task* task)
{
    enum { VIEW_FIGURE_LIGHT_COUNT = 3,
           VIEW_FIGURE_ANIM_IDLE   = 1 };
    VECTOR          lightingSample;
    ViewFigureWork* work;
    TmdObject*      model;
    GfxCoord*       rootCoord;

    model           = task->extra.tmd;
    rootCoord       = model->coords;
    work            = memCalloc(sizeof(ViewFigureWork), false);
    gViewFigureWork = work;
    task->work      = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _viewFigureExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    model->otOffset                  = 0;
    rootCoord->composeStamp          = GRAPHICS_COORD_DIRTY;
    gActorSelfTask                   = task;
    gActorHelperTask                 = taskSpawnFromTable(gViewFigureTasks, 1, 0, 0);
    animationInitContext(&gViewFigureWork->rig.anim, (AnimationSet**)gViewFigureAnimSets, model,
                         gViewFigureWork->rig.poses, gViewFigureWork->rig.slots);
    gViewFigureWork->st.animId = VIEW_FIGURE_ANIM_IDLE;
    gViewFigureWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    _viewFigureStepAnim(task);
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1];
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, VIEW_FIGURE_LIGHT_COUNT);
    gViewFigureWork->st.field_6++;
    task->msgTable = gViewFigureMessages;
    task->state++;
}
