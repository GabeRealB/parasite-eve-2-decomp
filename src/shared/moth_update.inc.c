/* Part of the Moth library; see moth.h. */

/// Task state 1. Shows or hides the model and target lock with the combat
/// actor-control state (paused: colour only). Otherwise runs the contact pass
/// and the part oscillation, becomes alerted (arming state F0) once another
/// moth has died, steers and drifts, recomposes the root coordinate, updates
/// the colour and plays sound 8 on a 1-in-128 roll.
void mothUpdate(Enemy* arg0, Task* arg1)
{
    TmdObject* obj;
    MothWork*  work;
    GfxCoord*  coord;
    s32        state;
    s32        one;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags                   = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case1:
    mothUpdateColor(arg1);
    return;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    mothContacts(arg1);
    mothOscillateParts(arg1);
    if (work->alerted == 0 && gSceneCombatState.actor00700DeathAlert != 0) {
        work->alerted = 1;
        Gp_ArmStateF0(1);
    }
    mothSteer(arg1);
    mothDrift(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    mothUpdateColor(arg1);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070008;
        temp = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(id, temp, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
}
