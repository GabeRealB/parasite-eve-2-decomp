/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback for the second enemy form, the five-state counterpart
/// of `madChaserCombatTick`: in mode 0 it aims (`madChaserTrackPlayer`),
/// lets a pending hit replace the state handler, rebuilds the root rotation,
/// then picks the next state - 4 when dead, 8 / 9 for messages 4 / 5, and
/// state 3 after a consumed `hitReaction` request. Mode 1 only recolours; both
/// clear bit 0x80 of the model's `field_C`, which mode 2 sets.
void madChaserLurkTick(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    Enemy*         enemy = arg0->spawnArg2.pointer;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    GfxCoord*      coord = obj->coords;
    TaskFuncTable5 sp    = gMadChaserLurkStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            madChaserTrackPlayer(arg0);
            if (madChaserTakeHit(arg0) == 0) {
                sp.funcs[(s16)work->state](arg0);
            }
            _madChaserTickAnim(arg0);
            madChaserTwistSpine(arg0);
            madChaserUpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_SHRINK_DEATH);
            } else if (_madChaserTakeHitReaction(arg0)) {
                work->busy = 0;
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_COMBAT);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
