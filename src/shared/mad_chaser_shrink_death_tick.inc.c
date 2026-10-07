#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback, the seven-state counterpart of
/// `madChaserDeathTick`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
void madChaserShrinkDeathTick(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    GfxCoord*      coord = obj->coords;
    TaskFuncTable7 sp    = gMadChaserShrinkDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            sp.funcs[(s16)work->state](arg0);
            if (!(work->frameCount & 0x1F)) {
                effectSpawnHit(EFFECT_HIT_KIND_BLAST, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
