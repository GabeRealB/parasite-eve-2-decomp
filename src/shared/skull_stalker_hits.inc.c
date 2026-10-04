/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `bodyContacts` records to the root coordinate (restoring `prevRootPos` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
void skullStalkerHits(Task* arg0)
{
    SkullStalkerWork*         work;
    ActorContactDeltaScratch* sc;
    ActorContactDeltaScratch* head;
    TmdObject*                obj;
    GfxCoord*                 coord;
    Enemy*                    enemy;
    s32                       i;
    s32                       sndHit;
    s32                       sndHit2;
    u32                       damage;
    s32                       snd;

    work                                           = arg0->work;
    head                                           = SCRATCH_STACK_CURSOR(ActorContactDeltaScratch);
    SCRATCH_STACK_CURSOR(ActorContactDeltaScratch) = head - 1;
    sc                                             = head - 1;
    obj                                            = arg0->extra.tmd;
    coord                                          = obj->coords;
    enemy                                          = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->bodyContacts, &head[-1].delta, ARRAY_SIZE(work->bodyContacts), NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += sc->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += sc->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->bodyContacts[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->variant != 0) {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x200, &gSkullStalkerSparkOffset);
                Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x200, &gSkullStalkerSparkOffset);
                Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg0->extra.tmd->coords, 0, &gSkullStalkerHitFxOffset);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->flattenScaleY = 0x500;
                work->animId        = SKULL_STALKER_ANIM_IDLE;
                enemy->hp           = 0;
                work->hiding        = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                damage              = Gp_ComputeDamage(work->bodyContacts[i].key.value,
                                                       SquareRoot0(sc->delta.vector.vx * sc->delta.vector.vx +
                                                                   sc->delta.vector.vy * sc->delta.vector.vy +
                                                                   sc->delta.vector.vz * sc->delta.vector.vz),
                                                       0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->bodyContacts[i].key.value, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78(enemy, work->bodyContacts[i].key.value, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->variant != 0) {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x200, &gSkullStalkerSparkOffset);
                    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x200, &gSkullStalkerSparkOffset);
                    Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg0->extra.tmd->coords, 0, &gSkullStalkerHitFxOffset);
                    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->flattenScaleY = 0x1000;
                    work->hiding        = 1;
                    work->animId        = SKULL_STALKER_ANIM_IDLE;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->bodyContacts[i].key.value)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2(enemy, work->bodyContacts[i].key.value, 0);
                        break;
                    case 8:
                        work->hiding = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < ARRAY_SIZE(work->bodyContacts));
    Gp_ClearRec18Occupied(work->bodyContacts);
    SCRATCH_STACK_CURSOR(ActorContactDeltaScratch) = SCRATCH_STACK_CURSOR(ActorContactDeltaScratch) + 1;
}
