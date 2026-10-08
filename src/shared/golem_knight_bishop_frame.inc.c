/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Updates the active GOLEM's combat behaviour, animation and presentation.
///
/// `enemy` belongs to `task`, whose model and GOLEM work are initialized.
/// Running mode derives target eligibility from the hurt body's pair-enable
/// bit. Paused mode draws tint/shadow without advancing behaviour; hidden mode
/// suppresses drawing and targeting. A missing room-region table skips updates.
/// Motion and animation precede sound cues, composition and appearance updates;
/// the refracted-frame capture uses part 3's depth with the shared depth bias.
static void _golemKnightBishopFrameState(Enemy* enemy, Task* task)
{
    GolemKnightBishopWork* work;
    TmdObject*             model;
    GfxCoord*              root;

    work  = task->work;
    model = task->extra.tmd;
    root  = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->fadeState != GOLEM_KNIGHT_BISHOP_FADE_HIDDEN) {
                model->flags = 0;
            }
            enemy->node.state.parts.flags = (work->hurtBody.flags & WORLD_COLLISION_BODY_PAIR_ENABLED) ? 0 : WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _golemKnightBishopUpdateTint(task);
            _golemKnightBishopDrawShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->regions != NULL) {
        // Resolve reactions and movement before advancing the requested animation.
        _golemKnightBishopTakeHits(task);
        _golemKnightBishopRunSequence(task);
        _golemKnightBishopStepForward(task);
        _golemKnightBishopTickAnim(task);
        _golemKnightBishopPlayAnimCues(task);
        root->composeStamp                      = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(root);
        _golemKnightBishopUpdateTint(task);
        _golemKnightBishopDrawShadow(task);
        _golemKnightBishopQueueFrameCapture(&task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_FRAME_CAPTURE_BIAS);
        _golemKnightBishopUpdateAppearance(task);
        modelLightingSetLayerMaterials(work->translucency);
    }
}
