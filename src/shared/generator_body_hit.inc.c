/* Part of the Generator library; see generator.h. */

/// Scratch-stack block of the body's hit handler, reserved for the length of
/// the call.
typedef struct {
    VECTOR  toPlayer;     // the player's position less the body's root, the range a hit's damage is computed from
    SVECTOR effectOffset; // where a hit effect appears, as an offset from the body's root: the kind's entry of the hit-effect offsets
} _GeneratorBodyHitScratch;
STATIC_ASSERT_SIZEOF(_GeneratorBodyHitScratch, 0x18);

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
    _GeneratorBodyHitScratch* scr;
    GeneratorWork*            work;
    Enemy*                    enemy;
    GfxCoord*                 coord;
    s32                       damage;
    s32                       lastId;
    s32                       val;
    s32                       snd;
    s32                       i;

    scr    = SCRATCH_STACK_RESERVE_BLOCK(_GeneratorBodyHitScratch);
    coord  = arg0->extra.tmd->coords;
    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    lastId = 0;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if ((work->hitCooldown << 0x10) <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->hitCooldown == 0) {
        for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
            if ((work->contacts[i].key.value & 0xFFFF0000) != 0x20000) {
                continue;
            }
            scr->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            scr->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            scr->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage           = Gp_ComputeDamage(work->contacts[i].key.value, SquareRoot0(scr->toPlayer.vx * scr->toPlayer.vx + scr->toPlayer.vy * scr->toPlayer.vy + scr->toPlayer.vz * scr->toPlayer.vz), 0, 0);
            if (work->lifeSupportDestroyed == 0) {
                damage /= 10;
            } else if (damageRollCriticalHit(enemy, work->contacts[i].key.value, 0) != 0) {
                damage              *= 4;
                scr->effectOffset.vx = gGeneratorHitEffectOffsets[work->kind].vx;
                scr->effectOffset.vy = gGeneratorHitEffectOffsets[work->kind].vy;
                scr->effectOffset.vz = gGeneratorHitEffectOffsets[work->kind].vz;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &scr->effectOffset);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            damageAccumulateLifeDrainHp(enemy, work->contacts[i].key.value, damage, 0);
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
                lastId               = work->contacts[i].key.value;
                val                  = damageGetPlayerAttackEffectId(lastId);
                scr->effectOffset.vx = gGeneratorHitEffectOffsets[work->kind].vx;
                scr->effectOffset.vy = gGeneratorHitEffectOffsets[work->kind].vy;
                scr->effectOffset.vz = gGeneratorHitEffectOffsets[work->kind].vz;
                if (val == 3) {
                    Gp_SpawnEff(EFFECT_HIT_BLAST, coord, work->effectArg.spawnArgLo | (work->effectArg.spawnArgHi << 16), &scr->effectOffset);
                } else {
                    func_800FDB18((u16)val, coord, &scr->effectOffset, &work->effectArg);
                }
            }
            val = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
            if (val > 0) {
                work->hitCooldown = val;
            }
            snd = gGeneratorSoundIds[2] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(_GeneratorBodyHitScratch);
}
