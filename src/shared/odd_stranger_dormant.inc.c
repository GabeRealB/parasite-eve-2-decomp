/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Waits in `ODD_STRANGER_STATE_DORMANT_SCRIPTED` for the player or combat noise.
///
/// Installs the scripted dormant animation in the borrowed bank on entry,
/// then queues the Patio soundId once and emits the hit effect at cue 4.
/// Player proximity or noise selects `ODD_STRANGER_STATE_ALERT`; only the
/// proximity path stops the soundId. Requires live work, enemy and model.
static void _oddStrangerDormantScripted(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        root;
    SVECTOR          toPlayer;
    SVECTOR*         toPlayerPtr;
    s32              soundId;
    s32              pan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                                                    = task->extra.tmd;
        gOddStrangerAnimSets[ODD_STRANGER_ANIM_DORMANT_SCRIPTED] = &gOddStrangerDormantAnimSet;
        work->animId                                             = ODD_STRANGER_ANIM_DORMANT_SCRIPTED;
        work->animRequest                                        = ODD_STRANGER_ANIM_REQUEST_RESET;
        model->flags                                             = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
    } else if (work->stateTimer == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT;
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateTimer = 1;
    }
    _oddStrangerDriveAnimation(task);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        work->effectArg.coord      = task->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ODD_STRANGER_PART1_FX_SCALE;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), task->extra.tmd->coords + 5, NULL, &work->effectArg);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    root               = task->extra.tmd->coords;
    toPlayerPtr        = &toPlayer;
    toPlayer.vx        = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    toPlayerPtr->vy    = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
    toPlayerPtr->vz    = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    if (!_oddStrangerOutOfRange(toPlayerPtr, work->noticeRadius)) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sceneEngageBattle(1);
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
#if ODD_STRANGER_VARIANT == 1
        sceneEngageBattle(1);
#endif
        work->state = ODD_STRANGER_STATE_ALERT;
    }
}
