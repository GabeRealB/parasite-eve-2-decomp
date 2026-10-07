/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Detaches and hides the enemy before the timed vanish teardown.
///
/// Stops the alert cry with its release retained and clears the shared alert
/// owner only when this enemy owns it. Unlinks targeting and all three collision
/// bodies, detaches the enemy's borrowed contact records and hides active drawing.
/// Resets the frame counter and advances the behavior. Requires live work, model
/// and enemy; their allocations and the scene battle reference remain live.
static void _madChaserVanish(Task* task)
{
    MadChaserWork* collisionWork;
    MadChaserWork* work;
    Enemy*         enemy;
    TmdObject*     model;

    work              = task->work;
    enemy             = task->spawnArg2.pointer;
    model             = task->extra.tmd;
    work->stateFrames = 0;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    // Detach every list user before the later state releases storage.
    enemy->recs   = NULL;
    collisionWork = task->work;
    worldCollisionUnlinkBody(&collisionWork->pairBody);
    worldCollisionUnlinkBody(&collisionWork->gridBody);
    worldCollisionUnlinkBody(&collisionWork->attackBody);
    model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state  = work->state + 1;
}
