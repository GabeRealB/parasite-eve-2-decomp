/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback, the five-state counterpart of
/// `madChaserShrinkDeathTick`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
void madChaserDropDeathTick(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = gMadChaserDropDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
