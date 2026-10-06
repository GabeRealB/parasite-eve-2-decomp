/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 23: posts the package's slot-16 animation set and plays clip 0x10, queues the 0x51030008 sound once, spawns the 0x1001 effect on clip frame 4, and moves to state 6 when the player comes within noticeRadius or the noise/cast combat signal is up.
void oddStrangerDormant(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                      = arg0->extra.tmd;
        gOddStrangerAnimSets[16] = &gOddStrangerDormantAnimSet;
        work->animId             = 0x10;
        work->animRequest        = ODD_STRANGER_ANIM_REQUEST_RESET;
        obj->flags               = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
    } else if (work->stateTimer == 0) {
        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51030008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateTimer = 1;
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        work->effectArg.coord      = arg0->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ODD_STRANGER_PART1_FX_SCALE;
        work->effectArg.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord              = arg0->extra.tmd->coords;
    d                  = &delta;
    delta.vx           = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy              = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!oddStrangerOutOfRange(d, work->noticeRadius)) {
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
