#include "gameplay/room_effects.h"

/* Part of the Generator library; see generator.h. */

/// Processes the Life Support part's attack contact and starts teardown on death.
///
/// Requires initialized part work and a live parent GeneratorWork. Paused or
/// hidden actor control leaves contacts and timers intact. Attachment attacks
/// show a zero readout without dealing damage; weapon attacks use root-to-player
/// distance and fourfold critical damage. Surviving positive damage arms the
/// attack cooldown and rate-limited effects. A killing hit disables the body's
/// protection and regeneration immediately. Borrows one scratch VECTOR.
static void _generatorLifeSupportHit(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_LIFE_SUPPORT_ATTACHMENT_ATTACK   = 0x8000,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_FRAMES   = 10,
        GENERATOR_LIFE_SUPPORT_HIT_SOUND_INDEX     = 0,
        GENERATOR_LIFE_SUPPORT_BREAK_SOUND_INDEX   = 1,
        GENERATOR_LIFE_SUPPORT_BREAK_EXPLOSION_ARG = 0x10002400,
        GENERATOR_LIFE_SUPPORT_BREAK_SMOKE_ARG     = 0x32FF1400
    };
    VECTOR*                   toPlayer;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    s32                       damage;
    s32                       soundId;
    s32                       hitCooldownFrames;

    coord = task->extra.coordBody->coord;
    part  = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    toPlayer = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    if (part->hitCooldown != 0) {
        part->hitCooldown--;
        if (part->hitCooldown <= 0) {
            part->hitCooldown = 0;
        }
    }
    if (part->hitEffectCooldown != 0) {
        part->hitEffectCooldown--;
    }
    // The part rejects attachment/PE attacks without consuming a new cooldown.
    if (part->hitCooldown == 0 && (part->contacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
        if (part->contacts[0].key.value & GENERATOR_LIFE_SUPPORT_ATTACHMENT_ATTACK) {
            worldTargetAddReadoutAmount(&enemy->node, 0, 0);
        } else {
            toPlayer->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            toPlayer->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            toPlayer->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage       = damageComputePlayerAttack(part->contacts[0].key.value, SquareRoot0(toPlayer->vx * toPlayer->vx + toPlayer->vy * toPlayer->vy + toPlayer->vz * toPlayer->vz), 0, 0);
            if (damageRollCriticalHit(enemy, part->contacts[0].key.value, 0) != 0) {
                damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                // Drop the parent's protection now; the part tears down on later ticks.
                task->state                                                = GENERATOR_TASK_TEARDOWN;
                part->teardownFrames                                       = 0;
                ((GeneratorWork*)task->parent->work)->lifeSupportDestroyed = 1;
                effectSpawn(EFFECT_EXPLOSION, coord, GENERATOR_LIFE_SUPPORT_BREAK_EXPLOSION_ARG, NULL);
                effectSpawn(EFFECT_SMOKE_PUFF, coord, GENERATOR_LIFE_SUPPORT_BREAK_SMOKE_ARG, NULL);
                soundId  = gGeneratorSoundIds[GENERATOR_LIFE_SUPPORT_BREAK_SOUND_INDEX];
                soundId |= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (damage > 0) {
                if (part->hitEffectCooldown == 0) {
                    if ((u16)damageGetPlayerAttackReaction(part->contacts[0].key.value) == DAMAGE_PLAYER_REACTION_INCENDIARY) {
                        effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, NULL, &part->effectArg);
                    }
                    effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, NULL, &part->effectArg);
                    part->hitEffectCooldown = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_FRAMES;
                }
                hitCooldownFrames = damageGetPlayerAttackHitCooldown(part->contacts[0].key.value);
                if (hitCooldownFrames > 0) {
                    part->hitCooldown = hitCooldownFrames;
                }
                soundId  = gGeneratorSoundIds[GENERATOR_LIFE_SUPPORT_HIT_SOUND_INDEX];
                soundId |= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
    }
    worldCollisionClearContacts(part->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
