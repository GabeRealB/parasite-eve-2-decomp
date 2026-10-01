/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Per-frame callback shaped like `func_actor_341700_80164CDC`, with a
/// one-entry handler table. `gSceneCombatState.actorControl` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `hopperDrawLimbShadow` for three part pairs and unhide it. The work block is reloaded through its own local
/// for the state reset, as the original does.
void hopperDangleFrame(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    GfxCoord*        coord = obj->coords;
    TaskFunc         sp[1] = { hopperDangleState };

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            hopperApplyContacts(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                Actor341700Work* w = (Actor341700Work*)arg0->work;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            hopperTickAnim(arg0);
            if (work->field_432 == 1) {
                hopperPinPart(arg0, 6, (SVECTOR3*)&work->field_80);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
