/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Frame handler for the scene's `Gp_StateF0.field_4` mode. Mode 1 only refreshes the
/// tint and the ground shadow, and mode 2 hides the model; both return at once.
/// Mode 0 shows the model again while the `field_6DA` timer runs and makes the
/// enemy lockable only while a hit is pending (bit 0x8000 of `field_49A`).
/// Then, once the box table is set, the frame runs: the hit handler, the
/// sequence dispatch, the step forward, the animation reseed, the vocal cue,
/// the coordinate refresh, the tint and shadow, the projection at depth +0xC,
/// the fade and `func_8009EA50`.
void stalkerFrameState(Enemy* arg0, Task* arg1)
{
    Actor402200Work* temp_s1;
    TmdObject*       temp_a1;
    GfxCoord*        temp_s2;
    s32              state;
    s32              one;

    temp_s1 = arg1->work;
    temp_a1 = arg1->extra.tmd;
    temp_s2 = temp_a1->coords;
    state   = Gp_StateF0.field_4;
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
    if (temp_s1->field_6DA != 0) {
        temp_a1->flags = 0;
    }
    arg0->node.state.parts.flags = (temp_s1->field_49A >> 0xF) ^ WORLD_TARGET_NOT_LOCKABLE;
    goto default_body;
case1:
    stalkerUpdateTint(arg1);
    stalkerDrawShadow(arg1);
    return;
case2:
    temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (temp_s1->field_6B4 != 0) {
        stalkerTakeHits(arg1);
        stalkerRunSequence(arg1);
        stalkerStepForward(arg1);
        stalkerTickAnim(arg1);
        stalkerPlayAnimCues(arg1);
        temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
        arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(temp_s2);
        stalkerUpdateTint(arg1);
        stalkerDrawShadow(arg1);
        stalkerQueueFrameCapture(&arg1->extra.tmd->coords[3], 0xC);
        stalkerCloakFade(arg1);
        func_8009EA50(temp_s1->field_6D8);
    }
}
