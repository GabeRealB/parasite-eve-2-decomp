/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Updates the live specimen, with redraw-only pause and hidden-mode suppression.
///
/// Borrows the owning Enemy and live model task. RUNNING clears model/target
/// flags before behaviour, status reactions, contacts and animation; HIDDEN
/// sets draw suppression and makes the target unlockable; PAUSED only colours
/// and draws the shadow. Other mode values update without clearing those flags.
/// Running updates scale part 1 and compose its matrix before lighting; paused
/// redraw uses its existing cached matrix. No resource ownership changes.
static void _sucklercephUpdateState(Enemy* enemy, Task* task)
{
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags        = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _sucklercephColour(enemy, task);
            _sucklercephDrawShadow(task);
            return;
    }
    _sucklercephReactionDispatch(task);
    _sucklercephReactionFlags(task);
    _sucklercephContacts(task);
    _sucklercephAnimate(task);
    _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    _sucklercephColour(enemy, task);
    _sucklercephDrawShadow(task);
}
