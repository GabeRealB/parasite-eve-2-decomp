/* Part of the glow pod library; see glow_pod.h. */

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
void glowPodHits(Task* arg0)
{
    GlowPodWork*       work;
    ActorDeltaFrame38* sc;
    ActorDeltaFrame38* head;
    TmdObject*         obj;
    GfxCoord*          coord;
    Enemy*             enemy;
    s32                i;
    s32                sndHit;
    s32                sndHit2;
    u32                damage;
    s32                snd;

    work                                    = (GlowPodWork*)arg0->work;
    head                                    = SCRATCH_STACK_CURSOR(ActorDeltaFrame38);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = head - 1;
    sc                                      = head - 1;
    obj                                     = arg0->extra.tmd;
    coord                                   = obj->coords;
    enemy                                   = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.halves.integer;
            coord->coord.t[1] += sc->delta.vy.halves.integer;
            coord->coord.t[2] += sc->delta.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &gGlowPodSparkOffset);
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &gGlowPodSparkOffset);
                Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &gGlowPodHitFxOffset);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.word = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.word = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.word = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                damage            = Gp_ComputeDamage(work->field_1A4[i].key.value,
                                                     SquareRoot0(sc->delta.vx.word * sc->delta.vx.word +
                                                                 sc->delta.vy.word * sc->delta.vy.word +
                                                                 sc->delta.vz.word * sc->delta.vz.word),
                                                     0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->field_1A4[i].key.value, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78(enemy, work->field_1A4[i].key.value, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &gGlowPodSparkOffset);
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &gGlowPodSparkOffset);
                    Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &gGlowPodHitFxOffset);
                    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key.value)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2(enemy, work->field_1A4[i].key.value, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = SCRATCH_STACK_CURSOR(ActorDeltaFrame38) + 1;
}
