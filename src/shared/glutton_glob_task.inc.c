/* Part of the Glutton library; see glutton.h. */

#include "glutton_projectile_state.h"

/// Dispatches the catching glob and records entry into each task state.
///
/// Requires a live enemy in `spawnArg2.pointer`, TMD body and state 0..4;
/// every nonzero state requires projectile work. Paused ticks retain normal
/// drawing and skip dispatch; hidden ticks suppress drawing and dispatch.
/// Running ticks restore normal drawing. A handler may destroy the task.
/// Other control values dispatch without changing the model flags.
static void _gluttonGlobTask(Task* task)
{
    enum { GLUTTON_GLOB_SPAWN_STATE = 0 };
    EnemyTaskFuncTable5    states;
    GluttonProjectileWork* work;

    states = gGluttonGlobStates;
    work   = task->work;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            task->extra.tmd->flags = 0;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (task->state != GLUTTON_GLOB_SPAWN_STATE) {
        _gluttonRecordProjectileTaskState(work, task);
    }
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
