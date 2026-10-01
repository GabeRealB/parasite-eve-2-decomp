/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame tick: runs the state handler, integrates the forward step,
/// advances or reseeds the animation slots, then draws. The same body as
/// `Actor02000_Fn02A34` plus the dust effect.
void golemPawnRookFrameState(Enemy* ctx, Task* actor)
{
    TmdObject*       model;
    Actor105600Work* work;
    GfxCoord*        coord;

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
        golemPawnRookApplyReaction(actor);
    }
    golemPawnRookTakeHits(actor);
    gGolemPawnRookStates[work->field_6A6](actor);
    if (work->field_69E != 0) {
        golemPawnRookTurnTowardTarget(actor);
    }
    golemPawnRookStepRoot(actor);
    golemPawnRookTickAnim(actor);
    if (work->field_6B4 != 0) {
        golemPawnRookDecayHitTilt(actor);
    }
    golemPawnRookPlayAnimCues(actor);
    coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    if (work->field_6C4 == 0) {
        golemPawnRookSpawnDust(actor);
    }
    golemPawnRookDraw(actor, coord);
}
