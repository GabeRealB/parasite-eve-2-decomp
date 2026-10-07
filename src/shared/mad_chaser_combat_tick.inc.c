/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback for the main enemy, the eleven-state counterpart of
/// `madChaserEmergeTick`. In mode 0 it aims at the nearest actor
/// (`madChaserTrackPlayer`), lets a pending hit (`madChaserTakeHit`) replace the state
/// handler, eases `spineYaw` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `hitReaction` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `busy` is clear.
void madChaserCombatTick(Task* arg0)
{
    Enemy*          enemy = arg0->spawnArg2.pointer;
    TmdObject*      obj   = arg0->extra.tmd;
    MadChaserWork*  work  = (MadChaserWork*)arg0->work;
    GfxCoord*       coord = obj->coords;
    TaskFuncTable11 sp    = gMadChaserCombatStates;
    s32             cur;

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
            cur            = (u16)work->spineYaw;
            work->spineYaw = cur + ((s16)(-(cur * 16)) >> 9);
            madChaserTwistSpine(arg0);
            if (work->anchored == 1) {
                madChaserPinPart(arg0, 6, (SVECTOR3*)&work->leapAnchorPos);
            }
            madChaserUpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->leapCooldown != 0) {
                work->leapCooldown--;
            }
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                _madChaserEnterTaskState(arg0, work->hitReaction);
            }
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_SHRINK_DEATH);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            _madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            _madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            _madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}
