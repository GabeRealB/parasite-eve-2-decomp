#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs one room-command drop-death frame, including periodic blast effects.
///
/// Requires live enemy/model/work storage with at least nine coordinates and
/// work->state in 0..4. Hidden actors skip drawing and all frame work. Running
/// actors increment the s16 frame counter, dispatch one copied handler, spawn
/// a blast at part 1 every 32 counts and dirty the root. Running and paused
/// actors both update colour and optional limb shadows, then enable drawing.
static void _madChaserDropDeathTick(Task* task)
{
    enum { MAD_CHASER_DROP_DEATH_BLAST_INTERVAL_FRAMES = 32 };
    TmdObject*     model  = task->extra.tmd;
    MadChaserWork* work   = task->work;
    GfxCoord*      root   = model->coords;
    TaskFuncTable5 states = gMadChaserDropDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states.funcs[(s16)work->state](task);
            if (!(work->frameCount & (MAD_CHASER_DROP_DEATH_BLAST_INTERVAL_FRAMES - 1))) {
                effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            // Colour and shadows stay live while state progression is paused.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            }
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
