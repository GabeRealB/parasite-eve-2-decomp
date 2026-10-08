/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Advances the live enemy frame or redraws its paused presentation.
///
/// Requires successful setup and the owning enemy/task pair. PAUSED only
/// updates lighting and shadow; HIDDEN suppresses drawing and lock-on.
/// RUNNING restores both, while other control values perform the full frame
/// without that reset. Status and contacts precede behavior selection; burning,
/// turning, movement and animation precede root composition and presentation.
static void _maggotCaterpillarTick(Enemy* enemy, Task* actor)
{
    s32                    actorControl;
    TmdObject*             model;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;

    model        = actor->extra.tmd;
    actorControl = gSceneCombatState.actorControl;
    work         = actor->work;
    coord        = model->coords;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _maggotCaterpillarUpdateColor(actor);
            _maggotCaterpillarDrawShadow(actor);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Reactions may change both the behavior and the task's next-frame phase.
    if (enemy->reactionFlags != 0) {
        _maggotCaterpillarApplyStatus(actor);
    }
    _maggotCaterpillarResolveContacts(actor);
    _maggotCaterpillarRunBehaviour(actor);
    if (work->burning != 0) {
        _maggotCaterpillarBurnStep(actor);
    }
    if (work->turnRate != 0) {
        _maggotCaterpillarTurnStep(actor);
    }
    _maggotCaterpillarMoveStep(actor);
    _maggotCaterpillarTickAnim(actor);
    // Publish the final pose before querying lighting and drawing its shadow.
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _maggotCaterpillarUpdateColor(actor);
    _maggotCaterpillarDrawShadow(actor);
}
