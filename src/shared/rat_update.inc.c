/* Part of the Rat library; see rat.h. */

/// Runs one living-rat update and refreshes its composed root lighting/shadow.
///
/// Requires a live enemy/model and initialized work in update task state 1.
/// Running updates enable drawing/targeting, consume reactions and contacts,
/// dispatch behavior, turn, step and animate before composing the root. Paused
/// updates only sample cached lighting and draw the shadow; hidden updates
/// disable drawing and targeting. Other control values retain the model/target
/// flags and run the update. A fatal contact schedules the death task state.
static void _ratUpdate(Enemy* enemy, Task* actor)
{
    GfxCoord*  rootCoord;
    TmdObject* model;
    RatWork*   work;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = actor->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _ratUpdateColor(actor);
            _ratShadow(actor);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Contacts can select the next mode before behavior and movement run.
    if (enemy->reactionFlags != 0) {
        _ratReactions(actor);
    }
    _ratContacts(actor);
    _ratBehavior(actor);
    if (work->turnRate != 0) {
        _ratTurn(actor);
    }
    _ratStep(actor);
    _ratAnimate(actor);
    // Refresh the root cache after movement and animation, before lighting.
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _ratUpdateColor(actor);
    _ratShadow(actor);
}
