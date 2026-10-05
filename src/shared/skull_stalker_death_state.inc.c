/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Dying-state tick of the second enemy, under the `gSceneCombatState.actorControl` mode byte: 1
/// does nothing and 2 hides the model. Otherwise the root's matrix is saved
/// into `savedRootMtx` and refolded with the decaying Y scale. In the linger
/// phase the enemy is destroyed after 0x3D frames; before that, the kill
/// countdown running out releases state 0xF0, enters the linger phase and
/// unlinks the enemy's node and its three bodies, and the two animation slots
/// are rebound or advanced.
void skullStalkerDeathState(Enemy* arg0, Task* arg1)
{
    SkullStalkerWork* work;
    TmdObject*        obj;
    GfxCoord*         coord;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->deathPhase != SKULL_STALKER_DEATH_PHASE_COUNTDOWN) {
        work->savedRootMtx = coord->coord;
        skullStalkerFlatten(arg1);
        work->phaseFrames++;
        if (work->phaseFrames >= 0x3D) {
            enemyDestroy(arg0, arg1);
        }
        return;
    }
    work->savedRootMtx = coord->coord;
    skullStalkerFlatten(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add(arg1, 0x2F);
        work->deathPhase  = SKULL_STALKER_DEATH_PHASE_LINGER;
        work->phaseFrames = 0;
        arg0->recs        = 0;
        worldTargetUnlinkNode(&arg0->node);
        worldCollisionUnlinkBody(&work->senseBody);
        worldCollisionUnlinkBody(&work->frontSenseBody);
        worldCollisionUnlinkBody(&work->body);
    }
    skullStalkerTickAnim(arg1);
}
