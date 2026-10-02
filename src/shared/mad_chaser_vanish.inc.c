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

    work            = (MadChaserWork*)arg0->work;
    enemy           = (Enemy*)arg0->spawnArg2.pointer;
    model           = arg0->extra.tmd;
    work->field_412 = 0;
    SndEvt_EnqueueType7(SOUND_MAD_CHASER_ALERT_CRY, 1);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    enemy->recs = 0;
    work2       = (MadChaserWork*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->flags    = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_420 = work->field_420 + 1;
}
