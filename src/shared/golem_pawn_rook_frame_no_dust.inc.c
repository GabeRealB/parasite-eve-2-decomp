/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame tick of the packages without the dust effect: obeys
/// `gSceneCombatState.actorControl`, applies a pending reaction, runs the hit
/// tick and the `behavior` state handler, turns toward `targetYaw` while
/// `turnRate` is set, steps the root, ticks the animation, decays the hit tilt
/// while `hitTiltActive` is set, plays the voice cues, marks the root and part 3
/// dirty and draws. The same body as `golemPawnRookFrameState` without its
/// `screamCharges` dust spawn.
void golemPawnRookFrameStateNoDust(Enemy* ctx, Task* actor)
{
    TmdObject*         model;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    work  = actor->work;
    model = actor->extra.tmd;
    coord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                = 0;
            ctx->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            golemPawnRookDraw(actor, coord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }

    if (ctx->reactionFlags != 0) {
        _golemPawnRookApplyBuildupReaction(actor);
    }
    golemPawnRookTakeHits(actor);
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
    coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    golemPawnRookDraw(actor, coord);
}
