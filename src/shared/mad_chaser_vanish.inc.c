/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 2, releases `gSceneCombatState`'s hold if it is this enemy's,
/// unlinks the enemy node, detaches its records and unlinks its three hit
/// bodies, hides the model and advances the state.
void madChaserVanish(Task* arg0)
{
    MadChaserWork* work2;
    MadChaserWork* work;
    Enemy*         enemy;
    TmdObject*     model;

    work              = (MadChaserWork*)arg0->work;
    enemy             = (Enemy*)arg0->spawnArg2.pointer;
    model             = arg0->extra.tmd;
    work->stateFrames = 0;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    enemy->recs = 0;
    work2       = (MadChaserWork*)arg0->work;
    worldCollisionUnlinkBody(&work2->pairBody);
    worldCollisionUnlinkBody(&work2->gridBody);
    worldCollisionUnlinkBody(&work2->attackBody);
    model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->state  = work->state + 1;
}
