/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Frame handler for the scene's `gSceneCombatState.actorControl` mode. Mode 1 only refreshes the
/// tint and the ground shadow, and mode 2 hides the model; both return at once.
/// Mode 0 shows the model again unless `fadeState` is hidden and makes the
/// enemy lockable only while `hurtBody` is pair-enabled.
/// Then, in a room that has `regions`, the frame runs: the hit handler, the
/// sequence dispatch, the step forward, the animation reseed, the vocal cue,
/// the coordinate refresh, the tint and shadow, the projection at depth +0xC,
/// the fade and `modelLightingSetLayerMaterials` with `translucency`.
void golemKnightBishopFrameState(Enemy* arg0, Task* arg1)
{
    GolemKnightBishopWork* temp_s1;
    TmdObject*             temp_a1;
    GfxCoord*              temp_s2;
    s32                    state;
    s32                    one;

    temp_s1 = arg1->work;
    temp_a1 = arg1->extra.tmd;
    temp_s2 = temp_a1->coords;
    state   = gSceneCombatState.actorControl;
    one     = 1;
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
    if (temp_s1->fadeState != GOLEM_KNIGHT_BISHOP_FADE_HIDDEN) {
        temp_a1->flags = 0;
    }
    arg0->node.state.parts.flags = (temp_s1->hurtBody.flags >> 0xF) ^ WORLD_TARGET_NOT_LOCKABLE;
    goto default_body;
case1:
    golemKnightBishopUpdateTint(arg1);
    golemKnightBishopDrawShadow(arg1);
    return;
case2:
    temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (temp_s1->regions != 0) {
        golemKnightBishopTakeHits(arg1);
        golemKnightBishopRunSequence(arg1);
        golemKnightBishopStepForward(arg1);
        golemKnightBishopTickAnim(arg1);
        golemKnightBishopPlayAnimCues(arg1);
        temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
        arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(temp_s2);
        golemKnightBishopUpdateTint(arg1);
        golemKnightBishopDrawShadow(arg1);
        golemKnightBishopQueueFrameCapture(&arg1->extra.tmd->coords[3], 0xC);
        golemKnightBishopTranslucencyFade(arg1);
        modelLightingSetLayerMaterials(temp_s1->translucency);
    }
}
