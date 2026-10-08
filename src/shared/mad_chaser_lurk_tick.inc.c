/* Part of the Mad Chaser library; see mad_chaser.h. */

#include "mad_chaser_frame_shadows.inc.c"

/// Advances lurking behavior and contacts before selecting death or combat.
///
/// Requires live enemy/model/work, initialized nine-part animation and behavior
/// state 0..4. Running frames track the nearer player and dispatch unless a pull
/// or vanish command takes over, then animate, twist the spine, rebuild rotation
/// and apply contacts. Non-busy deaths take priority over a consumed hit reaction;
/// consumed hit latches enter combat at behavior/sub-state zero. Scripted deaths
/// require the extended task table. Paused frames still update color and ground
/// shadows. Both running and paused frames enable model drawing;
/// hidden frames suppress it and return. Borrows task-owned storage and requires
/// initialized rendering scratch and frame-arena space.
static void _madChaserLurkTick(Task* task)
{
    TmdObject*     model     = task->extra.tmd;
    Enemy*         enemy     = task->spawnArg2.pointer;
    MadChaserWork* work      = task->work;
    GfxCoord*      rootCoord = model->coords;
    TaskFuncTable5 states    = gMadChaserLurkStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _madChaserTrackPlayer(task);
            if (_madChaserTakePullOrVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            _madChaserTwistSpine(task);
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            // Contacts can end lurking; preserve death/command/reaction priority.
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_SHRINK_DEATH);
            } else if (_madChaserTakeHitReaction(task)) {
                work->busy = 0;
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            _madChaserDrawFrameShadows(task);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
