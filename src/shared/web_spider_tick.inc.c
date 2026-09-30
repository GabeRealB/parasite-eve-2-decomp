/* Part of the web spider library; see web_spider.h. */

/// Per-frame tick, selected by the global mode `Gp_StateF0.field_4`. Mode 1 only
/// updates the colour and the ground shadow; mode 2 sets the model's `field_C` to
/// 0x80 and the context's `field_14` to 1 and stops there; mode 0 clears both
/// and then runs the full tick like any other mode. The full tick applies the
/// timed status damage when the context flags ask for it, resolves the
/// collision records, runs the behaviour state, the effect step while
/// `field_3B0` is set and the turn step while `field_3A6` is, moves and
/// animates the actor and refreshes its coordinate.
void spiderTick(GpEnemy* arg0, Task* arg1)
{
    s32              state;
    TmdObject*       obj;
    Actor105500Work* work;
    GfxCoord*        coord;

    obj   = arg1->extra.tmd;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
    if (state == 1) {
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
    obj->flags                   = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        spiderApplyStatus(arg1);
    }
    spiderResolveContacts(arg1);
    spiderRunBehaviour(arg1);
    if (work->field_3B0 != 0) {
        spiderBurnStep(arg1);
    }
    if (work->field_3A6 != 0) {
        spiderTurnStep(arg1);
    }
    spiderMoveStep(arg1);
    spiderTickAnim(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    spiderUpdateColor(arg1);
    spiderDrawShadow(arg1);
}
