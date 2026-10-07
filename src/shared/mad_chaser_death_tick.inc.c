/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback of the main enemy. `gSceneCombatState.actorControl` 2 hides the model,
/// 0 runs the current state handler (then colours it), 1 only colours it.
/// Unless `shadowHidden` is set, it then runs `_madChaserDrawLimbShadow` for
/// three part pairs.
void madChaserDeathTick(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    GfxCoord*      coord = obj->coords;
    TaskFuncTable9 sp    = gMadChaserDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            sp.funcs[(s16)work->state](arg0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
