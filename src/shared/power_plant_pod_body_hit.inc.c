/* Part of the power plant pod library; see power_plant_pod.h. */

/// Hit handler of the main body, the first step of the tick. After the
/// cooldown `field_332` has run out, each of the two contact records the
/// player's attack claimed deals damage by distance: a tenth of it while the
/// part object is alive (`field_336` clear), in which case the body cannot
/// drop below 1 hit point, otherwise the full amount, quadrupled on a critical
/// roll. A surviving body enters its hit reaction (idle state 1, pose 2); a
/// killed one moves the task to its death handler, waiting in death state 3
/// with pose 3. An attack id differing from the previous record's spawns its
/// hit effect; every record restarts the cooldown from the id's parameter 2
/// and plays the hit sound.
void podBodyHit(Task* arg0)
{
    Actor05300Scratch* scr;
    Actor05300Work*    work;
    Enemy*             enemy;
    GfxCoord*          coord;
    s32                damage;
    s32                lastId;
    s32                val;
    s32                snd;
    s32                i;

    scr    = SCRATCH_STACK_RESERVE_BLOCK(Actor05300Scratch);
    coord  = arg0->extra.tmd->coords;
    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    lastId = 0;
    if (work->field_332 != 0) {
        work->field_332--;
        if ((work->field_332 << 0x10) <= 0) {
            work->field_332 = 0;
        }
        if (work->field_332 != 0) {
            goto end;
        }
    }
    for (i = 0; i < 2; i++) {
        if ((work->rec18[i].key.value & 0xFFFF0000) != 0x20000) {
            continue;
        }
        scr->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
        scr->delta.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        scr->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        damage        = Gp_ComputeDamage(work->rec18[i].key.value, SquareRoot0(scr->delta.vx * scr->delta.vx + scr->delta.vy * scr->delta.vy + scr->delta.vz * scr->delta.vz), 0, 0);
        if (work->field_336 == 0) {
            damage /= 10;
        } else if (Gp_RollEnemyChance(enemy, work->rec18[i].key.value, 0) != 0) {
            damage     *= 4;
            scr->ofs.vx = gPodHitEffectOffsets[work->field_334].vx;
            scr->ofs.vy = gPodHitEffectOffsets[work->field_334].vy;
            scr->ofs.vz = gPodHitEffectOffsets[work->field_334].vz;
            Gp_SpawnEff(0x6009C, coord, 0, &scr->ofs);
        }
        func_800DA6E8(&enemy->node, damage, 0);
        func_800E2C78(enemy, work->rec18[i].key.value, damage, 0);
        enemy->hp -= damage;
        if (enemy->hp <= 0) {
            if (work->field_336 == 0) {
                enemy->hp = 1;
            } else {
                arg0->state     = 2;
                work->field_32E = 3;
                work->field_330 = 2;
                work->field_338 = 0;
                work->field_320 = 3;
            }
        } else {
            work->field_32C = 1;
            work->field_328 = 0;
            work->field_320 = 2;
        }
        if (lastId != work->rec18[i].key.value) {
            lastId      = work->rec18[i].key.value;
            val         = Gp_GetIdParam1(lastId) & 0xFFFF;
            scr->ofs.vx = gPodHitEffectOffsets[work->field_334].vx;
            scr->ofs.vy = gPodHitEffectOffsets[work->field_334].vy;
            scr->ofs.vz = gPodHitEffectOffsets[work->field_334].vz;
            if (val == 3) {
                Gp_SpawnEff(0x6007F, coord, work->field_2F4.spawnArgLo | (work->field_2F4.spawnArgHi << 16), &scr->ofs);
            } else {
                func_800FDB18((u16)val, coord, &scr->ofs, &work->field_2F4);
            }
        }
        val = Gp_GetIdParam2(work->rec18[i].key.value);
        if (val > 0) {
            work->field_332 = val;
        }
        snd = gPodSoundIds[2] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
end:
    Gp_ClearRec18Occupied(work->rec18);
    SCRATCH_STACK_RELEASE_BLOCK(Actor05300Scratch);
}
