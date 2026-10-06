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
    switch (gSceneCombatState.actorControl) {
        case 0:
            arg0->node.state.parts.flags = 0;
            break;
        case 1:
            skullStalkerColour(arg0, arg1);
            return;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    arg1->extra.tmd->coords[0].coord.t[1] += 0x80;
    skullStalkerReactionDispatch(arg1);
    skullStalkerLightRamp(arg1);
    skullStalkerReactionFlags(arg1);
    skullStalkerHits(arg1);
    skullStalkerAnimate(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    skullStalkerColour(arg0, arg1);
}
