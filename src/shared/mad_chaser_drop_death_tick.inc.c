#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback, the five-state counterpart of
/// `madChaserShrinkDeathTick`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
void madChaserDropDeathTick(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    GfxCoord*      coord = obj->coords;
    TaskFuncTable5 sp    = gMadChaserDropDeathStates;

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
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
