/* Part of the Glutton library; see glutton.h. */

#include "glutton_projectile_state.h"

/// Dispatches a debris chunk and records entry into each task state.
///
/// Requires a live enemy in `spawnArg2.pointer`, TMD body and state 0..3;
/// states after spawn require projectile work. Paused ticks retain translucent
/// drawing and skip dispatch; hidden ticks suppress drawing and dispatch.
/// Running ticks use translucent drawing. A handler may destroy the task.
/// Other control values dispatch without changing the model flags.
static void _gluttonChunkTask(Task* task)
{
    EnemyTaskFuncTable4    states;
    GluttonProjectileWork* work;

    states = gGluttonChunkStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (task->work != NULL) {
        work = task->work;
        _gluttonRecordProjectileTaskState(work, task);
    }
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
