/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Per-frame handler of the first enemy under the `gSceneCombatState.actorControl` mode byte: mode
/// 1 runs only the tail, mode 2 hides the model and sets the node flag and
/// returns, mode 0 clears both before falling into the update, and any other
/// mode updates directly. The update runs the reaction dispatch, the flag
/// reactions, the contact handler and the animation rebind, scales the second
/// part, clears the display flags of the first two parts and recomputes the
/// second one's world matrix; the tail colours the enemy from that part and
/// draws its ground shadow.
void sucklercephUpdateState(Enemy* arg0, Task* arg1)
{
    switch (gSceneCombatState.actorControl) {
        case 0:
            arg1->extra.tmd->flags       = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = 1;
            return;
        case 1:
            _sucklercephColour(arg0, arg1);
            _sucklercephDrawShadow(arg1);
            return;
    }
    sucklercephReactionDispatch(arg1);
    sucklercephReactionFlags(arg1);
    sucklercephContacts(arg1);
    _sucklercephAnimate(arg1);
    _sucklercephScalePart(arg1, &arg1->extra.tmd->coords[1]);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    _sucklercephColour(arg0, arg1);
    _sucklercephDrawShadow(arg1);
}
