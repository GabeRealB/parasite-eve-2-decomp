/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback, the ten-state counterpart of
/// `_madChaserDeathTick`: in mode 0 a pending vanish command
/// (`_madChaserTakeVanishCommand`) replaces the state handler, and the root
/// rotation is rebuilt from `rotation` before
/// `madChaserApplyContacts`.
void madChaserEmergeTick(Task* arg0)
{
    TmdObject*      obj   = arg0->extra.tmd;
    MadChaserWork*  work  = (MadChaserWork*)arg0->work;
    GfxCoord*       coord = obj->coords;
    TaskFuncTable10 sp    = gMadChaserEmergeStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            if (_madChaserTakeVanishCommand(arg0) == 0) {
                sp.funcs[(s16)work->state](arg0);
            }
            _madChaserTickAnim(arg0);
            _madChaserUpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
