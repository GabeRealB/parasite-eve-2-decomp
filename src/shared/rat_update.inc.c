/* Part of the Rat library; see rat.h. */

/// Task state 1. Shows or hides the model and target lock with the combat
/// actor-control state (paused: only colour and shadow). Otherwise runs the
/// reaction handler when any reaction flag is set, the contact pass, the
/// behaviour dispatch, the turn (when the turn rate is non-zero), the step and
/// the animation, then recomposes the root coordinate and updates colour and
/// shadow.
void ratUpdate(Enemy* arg0, Task* arg1)
{
    GfxCoord*  coord;
    TmdObject* obj;
    RatWork*   work;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _ratUpdateColor(arg1);
            _ratShadow(arg1);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (arg0->reactionFlags != 0) {
        _ratReactions(arg1);
    }
    ratContacts(arg1);
    _ratBehavior(arg1);
    if (work->turnRate != 0) {
        _ratTurn(arg1);
    }
    _ratStep(arg1);
    _ratAnimate(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _ratUpdateColor(arg1);
    _ratShadow(arg1);
}
