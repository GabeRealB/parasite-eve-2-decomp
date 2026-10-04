/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Death: plays sound 3 unless the enemy's HP is already negative, releases
/// `gSceneCombatState`'s hold if it points at this enemy, unlinks the enemy node and
/// its three hit bodies, moves the task to state 5, tells slot-4 task 0 with
/// message 0x13F4, and hides the model.
void madChaserPulledIn(Task* arg0)
{
    MadChaserWork* objs;
    Enemy*         enemy;
    TmdObject*     tmd;
    MadChaserWork* work;
    s32            soundId;
    s32            pan;

    work       = (MadChaserWork*)arg0->work;
    enemy      = (Enemy*)arg0->spawnArg2.pointer;
    tmd        = arg0->extra.tmd;
    work->busy = 1;
    if (enemy->hp >= 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    objs        = (MadChaserWork*)arg0->work;
    Gp_UnlinkObj(&objs->pairBody);
    Gp_UnlinkObj(&objs->gridBody);
    Gp_UnlinkObj(&objs->attackBody);
    madChaserEnterState(arg0, 5);
    taskMessageDispatch(Gp_LookupSlot4(0), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
    tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}
