/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 23: posts the package's slot-16 animation set and plays clip 0x10, queues the 0x51030008 sound once, spawns the 0x1001 effect on clip frame 4, and moves to state 6 when the player comes within field_C16 or the noise/cast combat signal is up.
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
    if (work->field_4 != 0) {
        obj                      = arg0->extra.tmd;
        gOddStrangerAnimSets[16] = &gOddStrangerDormantAnimSet;
        work->field_89E          = 0x10;
        work->field_898          = 2;
        obj->flags               = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = ODD_STRANGER_BODY_RADIUS;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x10;
        work->field_8AE               = 0;
        work->field_6                 = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51030008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_6 = 1;
    }
    oddStrangerDrive(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = arg0->extra.tmd->coords + 1;
        work->field_8B8.spawnArgLo = ODD_STRANGER_PART1_FX_SCALE;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy           = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!oddStrangerOutOfRange(d, work->field_C16)) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
#if ODD_STRANGER_VARIANT == 1
        Gp_ArmStateF0(1);
#endif
        work->field_0 = 6;
    }
}
