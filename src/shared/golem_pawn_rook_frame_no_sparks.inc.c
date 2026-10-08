/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Runs a Pawn GOLEM combat frame without the Rook's damaged-body sparks.
///
/// enemy and actor must be the live body pair with initialized GOLEM work.
/// Running frames consume reactions and contacts before dispatching behavior,
/// then apply movement, animation, cues and lighting. Paused frames only sample
/// colour and draw the shadow from cached coordinates; hidden frames suppress
/// the model and lock-on. The behavior index must select a non-NULL carrier
/// handler. Work and collision storage remain owned by the body task.
static void _golemPawnRookFrameStateNoSparks(Enemy* enemy, Task* actor)
{
    TmdObject*         model;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    work  = actor->work;
    model = actor->extra.tmd;
    coord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _golemPawnRookUpdateColorAndDrawShadow(actor, coord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }

    // Consume last frame's contacts before choosing and advancing behavior.
    if (enemy->reactionFlags != 0) {
        _golemPawnRookApplyBuildupReaction(actor);
    }
    _golemPawnRookTakeHits(actor);
    gGolemPawnRookStates[work->behavior](actor);
    if (work->turnRate != 0) {
        _golemPawnRookTurnTowardTarget(actor);
    }
    _golemPawnRookStepRoot(actor);
    _golemPawnRookTickAnim(actor);
    if (work->hitTiltActive != 0) {
        _golemPawnRookDecayHitTilt(actor);
    }
    _golemPawnRookPlayAnimCues(actor);
    // Compose the updated pose before sampling colour and drawing its shadow.
    coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _golemPawnRookUpdateColorAndDrawShadow(actor, coord);
}
