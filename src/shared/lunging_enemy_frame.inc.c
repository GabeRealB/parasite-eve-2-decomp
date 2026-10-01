/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Per-frame tick: runs the state handler, integrates the forward step,
/// advances or reseeds the animation slots, then draws. The same body as
/// `Actor02000_Fn02A34` plus the dust effect.
void lungerFrameState(Enemy* ctx, Task* actor)
{
    TmdObject*       model;
    Actor105600Work* work;
    GfxCoord*        coord;

    work  = actor->work;
    model = actor->extra.tmd;
    coord = model->coords;
    switch (Gp_StateF0.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                = 0;
            ctx->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            lungerDraw(actor, coord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }

    if (ctx->reactionFlags != 0) {
        lungerApplyReaction(actor);
    }
    lungerTakeHits(actor);
    gLungerStates[work->field_6A6](actor);
    if (work->field_69E != 0) {
        lungerTurnTowardTarget(actor);
    }
    lungerStepRoot(actor);
    lungerTickAnim(actor);
    if (work->field_6B4 != 0) {
        lungerDecayHitTilt(actor);
    }
    lungerPlayAnimCues(actor);
    coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    if (work->field_6C4 == 0) {
        lungerSpawnDust(actor);
    }
    lungerDraw(actor, coord);
}
