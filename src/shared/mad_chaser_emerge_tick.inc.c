/* Part of the Mad Chaser library; see mad_chaser.h. */

#include "mad_chaser_frame_shadows.inc.c"

/// Advances room-entry behavior, animation and contacts, then refreshes presentation.
///
/// Requires the extended task table, live enemy/model/work, initialized nine-part
/// animation and behavior state 0..9. Running frames consume a vanish command
/// before dispatch, then tick animation, rebuild rotation and apply contacts.
/// A consumed command skips only the old behavior. Paused frames still update
/// color and draw ground shadows when shadowHidden is zero; hidden frames
/// suppress model drawing and return. Borrows task-owned storage and requires
/// initialized rendering scratch and frame-arena space.
static void _madChaserEmergeTick(Task* task)
{
    TmdObject*      model     = task->extra.tmd;
    MadChaserWork*  work      = task->work;
    GfxCoord*       rootCoord = model->coords;
    TaskFuncTable10 states    = gMadChaserEmergeStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            if (_madChaserTakeVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawFrameShadows(task);
            }
            return;
    }
}
