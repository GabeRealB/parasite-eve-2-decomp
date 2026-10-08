/* Part of the Mad Chaser library; see mad_chaser.h. */

#include "mad_chaser_frame_shadows.inc.c"

/// Advances the hanging body's behavior and keeps its anchor fixed after animation.
///
/// Requires live enemy/model/work and initialized nine-part animation. Dangle
/// uses behavior zero and sub-state 0..3. Running frames apply contacts while
/// ignoring the player's body, enter death on a fatal blast, tick animation and
/// pin part 6 to anchorPos when anchored. Paused frames still refresh color and
/// ground shadows. Both running and paused frames enable model drawing; hidden
/// frames suppress it and return. Borrows task-owned storage and requires
/// initialized rendering scratch and frame-arena space.
static void _madChaserDangleFrame(Task* task)
{
    enum { MAD_CHASER_DANGLE_ANCHOR_PART = 6 };
    TmdObject*     model     = task->extra.tmd;
    MadChaserWork* work      = task->work;
    Enemy*         enemy     = task->spawnArg2.pointer;
    GfxCoord*      rootCoord = model->coords;
    // The behavior dispatch remains indexed even though dangle only uses zero.
    TaskFunc states[1] = { _madChaserDangleState };

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states[(s16)work->state](task);
            _madChaserApplyContacts(task, 1);
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            }
            // Animate before translating the root to retain the hanging anchor.
            _madChaserTickAnim(task);
            if (work->anchored == 1) {
                _madChaserPinPart(task, MAD_CHASER_DANGLE_ANCHOR_PART, &work->anchorPos);
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
