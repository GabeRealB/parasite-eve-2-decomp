/* Part of the Glutton library; see glutton.h. */

#ifndef GLUTTON_GLOB_STATES
#error "define GLUTTON_GLOB_STATES as the carrier's EnemyTaskFuncTable5 object before including glutton.inc.c"
#endif

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

    states = GLUTTON_GLOB_STATES;
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

#undef GLUTTON_GLOB_STATES

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

    states = _gGluttonChunkStates;

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

/// Dispatches the rain blob's five-state coordinate-body lifecycle.
///
/// Requires a live enemy in `spawnArg2.pointer`, coordinate body and state
/// 0..4; states after spawn require projectile work. Paused and hidden actor
/// control skip dispatch; running and other control values dispatch normally.
/// A handler may destroy the task.
static void _gluttonRainTask(Task* task)
{
    EnemyTaskFuncTable5 states;

    states = _gGluttonRainStates;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            states.funcs[task->state](task->spawnArg2.pointer, task);
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}

/// Keeps a spinner visible on the floor until the boss releases the set.
///
/// Requires the spinner's live TMD body. Release advances its task to the chase
/// state; fight end destroys the enemy. Clearing the model flags enables normal
/// drawing, including on the tick the state advances.
static void _gluttonSpinnerWait(Enemy* enemy, Task* task)
{
    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        enemyDestroy(enemy, task);
        return;
    }

    if (_gGluttonSpinnersReleased == GLUTTON_SPINNER_RELEASE_ON) {
        task->state++;
    }

    task->extra.tmd->flags = 0;
}
