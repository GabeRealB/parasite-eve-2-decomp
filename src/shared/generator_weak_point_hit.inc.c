#include "gameplay/room_effects.h"

/* Part of the Generator library; see generator.h. */

/// Per-frame hit handling of the weak point: distance-scaled damage with
/// critical rolls and hit effects/sounds. On death it marks the body's
/// `lifeSupportDestroyed`, spawns the burst effects and plays the break sound.
void generatorLifeSupportHit(Enemy* arg0, Task* arg1)
{
    VECTOR*                   vec;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    s32                       damage;
    s32                       snd;
    s32                       hitTime;

    coord = arg1->extra.tmd->coords;
    part  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    vec = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    if (part->hitCooldown != 0) {
        part->hitCooldown--;
        if (part->hitCooldown <= 0) {
            part->hitCooldown = 0;
        }
    }
    if (part->hitEffectCooldown != 0) {
        part->hitEffectCooldown--;
    }
    if (part->hitCooldown == 0 && (part->contacts[0].key.value & 0xFFFF0000) == 0x20000) {
        if (part->contacts[0].key.value & 0x8000) {
            worldTargetAddReadoutAmount(&arg0->node, 0, 0);
        } else {
            vec->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage  = damageComputePlayerAttack(part->contacts[0].key.value, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (damageRollCriticalHit(arg0, part->contacts[0].key.value, 0) != 0) {
                damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
            }
            worldTargetAddReadoutAmount(&arg0->node, damage, 0);
            arg0->hp -= damage;
            if (arg0->hp <= 0) {
                arg1->state                                                = 2;
                part->teardownFrames                                       = 0;
                ((GeneratorWork*)arg1->parent->work)->lifeSupportDestroyed = 1;
                effectSpawn(EFFECT_EXPLOSION, coord, 0x10002400, NULL);
                effectSpawn(EFFECT_SMOKE_PUFF, coord, 0x32FF1400, NULL);
                snd  = gGeneratorSoundIds[1];
                snd |= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (damage > 0) {
                if (part->hitEffectCooldown == 0) {
                    if ((damageGetPlayerAttackReaction(part->contacts[0].key.value) & 0xFFFF) == DAMAGE_PLAYER_REACTION_INCENDIARY) {
                        effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, NULL, &part->effectArg);
                    }
                    effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, NULL, &part->effectArg);
                    part->hitEffectCooldown = 10;
                }
                hitTime = damageGetPlayerAttackHitCooldown(part->contacts[0].key.value);
                if (hitTime > 0) {
                    part->hitCooldown = hitTime;
                }
                snd  = gGeneratorSoundIds[0];
                snd |= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
    }
    worldCollisionClearContacts(part->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
