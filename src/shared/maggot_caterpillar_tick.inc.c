/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Per-frame tick, selected by the global mode `gSceneCombatState.actorControl`. Mode 1 only
/// updates the colour and the ground shadow; mode 2 sets the model's `field_C` to
/// 0x80 and the context's `field_14` to 1 and stops there; mode 0 clears both
/// and then runs the full tick like any other mode. The full tick applies the
/// timed status damage when the context flags ask for it, resolves the
/// collision records, runs the behaviour state, the effect step while
/// `burning` is set and the turn step while `turnRate` is, moves and
/// animates the actor and refreshes its coordinate.
void maggotCaterpillarTick(Enemy* arg0, Task* arg1)
{
    s32                    state;
    TmdObject*             obj;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    if (state == 1) {
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
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        maggotCaterpillarApplyStatus(arg1);
    }
    maggotCaterpillarResolveContacts(arg1);
    maggotCaterpillarRunBehaviour(arg1);
    if (work->burning != 0) {
        maggotCaterpillarBurnStep(arg1);
    }
    if (work->turnRate != 0) {
        maggotCaterpillarTurnStep(arg1);
    }
    maggotCaterpillarMoveStep(arg1);
    maggotCaterpillarTickAnim(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
case1:
    maggotCaterpillarUpdateColor(arg1);
    maggotCaterpillarDrawShadow(arg1);
}
