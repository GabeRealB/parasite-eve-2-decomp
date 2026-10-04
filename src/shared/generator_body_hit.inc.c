/* Part of the Generator library; see generator.h. */

/// Hit handler of the main body, the first step of the tick. After the
/// cooldown `hitCooldown` has run out, each of the two contact records the
/// player's attack claimed deals damage by distance: a tenth of it while the
/// part object is alive (`lifeSupportDestroyed` clear), in which case the body cannot
/// drop below 1 hit point, otherwise the full amount, quadrupled on a critical
/// roll. A surviving body enters its hit reaction (idle state 1, pose 2); a
/// killed one moves the task to its death handler, waiting in death state 3
/// with pose 3. An attack id differing from the previous record's spawns its
/// hit effect; every record restarts the cooldown from the id's parameter 2
/// and plays the hit sound.
void generatorBodyHit(Task* arg0)
{
    GeneratorScratch* scr;
    GeneratorWork*    work;
    Enemy*            enemy;
    GfxCoord*         coord;
    s32               damage;
    s32               lastId;
    s32               val;
    s32               snd;
    s32               i;

    scr    = SCRATCH_STACK_RESERVE_BLOCK(GeneratorScratch);
    coord  = arg0->extra.tmd->coords;
    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    lastId = 0;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if ((work->hitCooldown << 0x10) <= 0) {
            work->hitCooldown = 0;
        }
        if (work->hitCooldown != 0) {
            goto end;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((work->contacts[i].key.value & 0xFFFF0000) != 0x20000) {
            continue;
        }
        scr->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        scr->delta.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        scr->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        damage        = Gp_ComputeDamage(work->contacts[i].key.value, SquareRoot0(scr->delta.vx * scr->delta.vx + scr->delta.vy * scr->delta.vy + scr->delta.vz * scr->delta.vz), 0, 0);
        if (work->lifeSupportDestroyed == 0) {
            damage /= 10;
        } else if (Gp_RollEnemyChance(enemy, work->contacts[i].key.value, 0) != 0) {
            damage     *= 4;
            scr->ofs.vx = gGeneratorHitEffectOffsets[work->kind].vx;
            scr->ofs.vy = gGeneratorHitEffectOffsets[work->kind].vy;
            scr->ofs.vz = gGeneratorHitEffectOffsets[work->kind].vz;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &scr->ofs);
        }
        func_800DA6E8(&enemy->node, damage, 0);
        func_800E2C78(enemy, work->contacts[i].key.value, damage, 0);
        enemy->hp -= damage;
        if (enemy->hp <= 0) {
            if (work->lifeSupportDestroyed == 0) {
                enemy->hp = 1;
            } else {
                arg0->state           = 2;
                work->deathState      = GENERATOR_DEATH_WAIT;
                work->battleExitState = GENERATOR_BATTLE_EXIT_HELD;
                work->alive           = 0;
                work->animSet         = GENERATOR_ANIM_DEATH;
            }
        } else {
            work->pulseState  = GENERATOR_PULSE_HIT;
            work->stateFrames = 0;
            work->animSet     = GENERATOR_ANIM_HIT;
        }
        if (lastId != work->contacts[i].key.value) {
            lastId      = work->contacts[i].key.value;
            val         = Gp_GetIdParam1(lastId) & 0xFFFF;
            scr->ofs.vx = gGeneratorHitEffectOffsets[work->kind].vx;
            scr->ofs.vy = gGeneratorHitEffectOffsets[work->kind].vy;
            scr->ofs.vz = gGeneratorHitEffectOffsets[work->kind].vz;
            if (val == 3) {
                Gp_SpawnEff(EFFECT_HIT_BLAST, coord, work->effectArg.spawnArgLo | (work->effectArg.spawnArgHi << 16), &scr->ofs);
            } else {
                func_800FDB18((u16)val, coord, &scr->ofs, &work->effectArg);
            }
        }
        val = Gp_GetIdParam2(work->contacts[i].key.value);
        if (val > 0) {
            work->hitCooldown = val;
        }
        snd = gGeneratorSoundIds[2] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
end:
    Gp_ClearRec18Occupied(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(GeneratorScratch);
}
