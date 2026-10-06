/* Part of the Moth library; see moth.h. */

/// Task state 1. Shows or hides the model and target lock with the combat
/// actor-control state (paused: colour only). Otherwise runs the contact pass
/// and the part oscillation, becomes alerted (engaging battle) once another
/// moth has died, steers and drifts, recomposes the root coordinate, updates
/// the colour and plays sound 8 on a 1-in-128 roll.
void mothUpdate(Enemy* arg0, Task* arg1)
{
    TmdObject* obj;
    MothWork*  work;
    GfxCoord*  coord;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case 0:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 1:
            mothUpdateColor(arg1);
            return;
        case 2:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    mothContacts(arg1);
    mothOscillateParts(arg1);
    if (work->alerted == 0 && gSceneCombatState.actor00700DeathAlert != 0) {
        work->alerted = 1;
        sceneEngageBattle(1);
    }
    mothSteer(arg1);
    mothDrift(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    mothUpdateColor(arg1);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070008;
        temp = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        sndEvtRequestScriptStart(id, temp, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
}
