/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Per-frame callback shaped like `func_actor_341700_80164CDC`, with a
/// one-entry handler table. `gSceneCombatState.actorControl` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `hitReaction` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `madChaserDrawLimbShadow` for three part pairs and unhide it. The work block is reloaded through its own local
/// for the state reset, as the original does.
void madChaserDangleFrame(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    Enemy*         enemy = arg0->spawnArg2.pointer;
    GfxCoord*      coord = obj->coords;
    TaskFunc       sp[1] = { madChaserDangleState };

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            sp[(s16)work->state](arg0);
            madChaserApplyContacts(arg0, 1);
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                MadChaserWork* w = (MadChaserWork*)arg0->work;

                arg0->state = work->hitReaction;
                w->state    = 0;
                w->subState = 0;
            }
            _madChaserTickAnim(arg0);
            if (work->anchored == 1) {
                madChaserPinPart(arg0, 6, (SVECTOR3*)&work->anchorPos);
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
