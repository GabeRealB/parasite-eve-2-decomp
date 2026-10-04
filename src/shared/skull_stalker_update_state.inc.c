/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Per-frame handler of the second enemy under the `gSceneCombatState.actorControl` mode byte:
/// mode 1 runs only the tail, mode 2 hides the model, sets the node flag and
/// returns, mode 0 clears the node flag before falling into the update, and
/// any other mode updates directly. The update raises the root's Y translation
/// by 0x80, runs the reaction dispatch, the light blend, the flag reactions,
/// the hit handler and the animation, clears the first two parts' flags and
/// recomputes the second one's matrix; the tail colours the enemy.
void skullStalkerUpdateState(Enemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = gSceneCombatState.actorControl;
    one   = 1;
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
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    arg1->extra.tmd->coords[0].coord.t[1] += 0x80;
    skullStalkerReactionDispatch(arg1);
    skullStalkerLightRamp(arg1);
    skullStalkerReactionFlags(arg1);
    skullStalkerHits(arg1);
    skullStalkerAnimate(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
case1:
    skullStalkerColour(arg0, arg1);
}
