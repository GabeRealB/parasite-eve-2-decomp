/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback for the main enemy, the eleven-state counterpart of
/// `madChaserEmergeTick`. In mode 0 it aims at the nearest actor
/// (`madChaserTrackPlayer`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
void madChaserCombatTick(Task* arg0)
{
    Enemy*           enemy = arg0->spawnArg2.pointer;
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable11  sp    = gMadChaserCombatStates;
    s32              cur;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            madChaserTrackPlayer(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            madChaserTwistSpine(arg0);
            if (work->field_432 == 1) {
                madChaserPinPart(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            update_rotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                madChaserEnterState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                madChaserEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                madChaserEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                madChaserEnterState(arg0, 9);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}
