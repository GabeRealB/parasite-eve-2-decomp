/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Updates active behavior, visibility, contacts, animation and model color.
///
/// Paused actor control updates only color; hidden control disables drawing and
/// lock-on and returns. Running control clears the target flags before the
/// update; other values also update. Each active tick raises the root by 128
/// parent-coordinate units, then processes behavior and the fade before pending
/// reactions and hits. Coordinate 1 is recomposed before sampling its lighting.
static void _skullStalkerUpdateState(Enemy* enemy, Task* task)
{
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _skullStalkerUpdateColor(enemy, task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    task->extra.tmd->coords[0].coord.t[1] += 128;
    // Keep fade decisions ahead of hit/reaction changes and animation advancement.
    _skullStalkerUpdateBehavior(task);
    _skullStalkerUpdateVisibility(task);
    _skullStalkerConsumeReactions(task);
    _skullStalkerProcessContacts(task);
    _skullStalkerAnimate(task);
    // Refresh the lighting sample after playback updates the model parts.
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    _skullStalkerUpdateColor(enemy, task);
}
