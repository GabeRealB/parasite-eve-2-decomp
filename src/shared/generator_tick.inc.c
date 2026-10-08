/* Part of the Generator library; see generator.h. */

/// Runs one active body update subject to the scene's actor-control mode.
///
/// Running mode restores drawing and hides the HP readout. Paused mode only
/// updates color; hidden mode suppresses drawing and lock-on. Other values
/// run the full update while retaining drawing and target flags. Damage,
/// pulse scaling and animation precede coordinate composition, lighting and
/// regeneration; a killing hit still completes this frame before death dispatch.
static void _generatorTickState(Enemy* enemy, Task* task)
{
    GfxCoord*  rootCoord;
    TmdObject* bodyModel;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            bodyModel->flags              = 0;
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _generatorUpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            bodyModel->flags              = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        default:
            break;
    }
    // Finish the active frame even if damage switches the next dispatch to death.
    _generatorBodyHit(task);
    _generatorPulse(task);
    _generatorUpdateAnimation(task);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _generatorUpdateColor(task);
    _generatorRegenerate(task);
}
