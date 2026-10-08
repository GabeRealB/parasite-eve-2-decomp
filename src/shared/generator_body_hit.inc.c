#include "gameplay/room_effects.h"

/* Part of the Generator library; see generator.h. */

/// Scratch-stack block of the body's hit handler, reserved for the length of
/// the call.
typedef struct {
    VECTOR  toPlayer;     // the player's position less the body's root, the range a hit's damage is computed from
    SVECTOR effectOffset; // where a hit effect appears, as an offset from the body's root: the kind's entry of the hit-effect offsets
} _GeneratorBodyHitScratch;
STATIC_ASSERT_SIZEOF(_GeneratorBodyHitScratch, 0x18);

/// Applies body HP loss and chooses the protected floor, hit reaction or held death.
///
/// Requires the initialized body task and its work and Enemy. Retains the
/// original positive-HP reaction even for zero damage. No resource is released.
/// A protected lethal hit restores one HP without starting the hit animation;
/// an unprotected lethal hit leaves battle rewards and teardown held for messages.
static inline void _generatorApplyBodyDamage(Task* task, GeneratorWork* work, Enemy* enemy, s32 damage)
{
    enum { GENERATOR_PROTECTED_HP_FLOOR = 1 };

    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        if (work->lifeSupportDestroyed == 0) {
            enemy->hp = GENERATOR_PROTECTED_HP_FLOOR;
        } else {
            task->state           = GENERATOR_TASK_TEARDOWN;
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
}

/// Applies queued player attacks to the generator body and consumes its contacts.
///
/// Requires a live model task with an Enemy and GeneratorWork. Both contact
/// slots are checked after the signed frame cooldown expires. Life Support
/// reduces damage to one tenth and preserves at least one HP; after it breaks,
/// critical hits deal four times the damage and a killing hit starts the held
/// death sequence. Effects are suppressed only for consecutive equal attack
/// keys; every processed contact can arm a cooldown and play a sound.
/// Reserves one _GeneratorBodyHitScratch block for this call.
static void _generatorBodyHit(Task* task)
{
    enum {
        GENERATOR_PROTECTED_DAMAGE_DIVISOR = 10,
        GENERATOR_BODY_HIT_SOUND_INDEX     = 2
    };
    _GeneratorBodyHitScratch* scratch;
    GeneratorWork*            work;
    Enemy*                    enemy;
    GfxCoord*                 rootCoord;
    s32                       damage;
    s32                       lastAttackKey;
    s32                       hitParameter;
    s16                       cooldownFrames;
    s32                       soundId;
    s32                       contactIndex;

    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_GeneratorBodyHitScratch);
    rootCoord     = task->extra.tmd->coords;
    work          = task->work;
    enemy         = task->spawnArg2.pointer;
    lastAttackKey = 0;
    if (work->hitCooldown != 0) {
        cooldownFrames    = work->hitCooldown - 1;
        work->hitCooldown = cooldownFrames;
        if (cooldownFrames <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->hitCooldown == 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
            if ((work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
                continue;
            }
            // Measure damage in the player and root coordinates' common parent frame.
            scratch->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
            scratch->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            damage               = damageComputePlayerAttack(work->contacts[contactIndex].key.value, SquareRoot0(scratch->toPlayer.vx * scratch->toPlayer.vx + scratch->toPlayer.vy * scratch->toPlayer.vy + scratch->toPlayer.vz * scratch->toPlayer.vz), 0, 0);
            if (work->lifeSupportDestroyed == 0) {
                damage /= GENERATOR_PROTECTED_DAMAGE_DIVISOR;
            } else if (damageRollCriticalHit(enemy, work->contacts[contactIndex].key.value, 0) != 0) {
                damage                  *= 4;
                scratch->effectOffset.vx = gGeneratorHitEffectOffsets[work->kind].vx;
                scratch->effectOffset.vy = gGeneratorHitEffectOffsets[work->kind].vy;
                scratch->effectOffset.vz = gGeneratorHitEffectOffsets[work->kind].vz;
                effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, 0, &scratch->effectOffset);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, damage, 0);
            _generatorApplyBodyDamage(task, work, enemy, damage);
            if (lastAttackKey != work->contacts[contactIndex].key.value) {
                lastAttackKey            = work->contacts[contactIndex].key.value;
                hitParameter             = damageGetPlayerAttackEffectId(lastAttackKey);
                scratch->effectOffset.vx = gGeneratorHitEffectOffsets[work->kind].vx;
                scratch->effectOffset.vy = gGeneratorHitEffectOffsets[work->kind].vy;
                scratch->effectOffset.vz = gGeneratorHitEffectOffsets[work->kind].vz;
                if (hitParameter == EFFECT_HIT_KIND_BLAST) {
                    effectSpawn(EFFECT_HIT_BLAST, rootCoord, work->effectArg.spawnArgLo | (work->effectArg.spawnArgHi << 16), &scratch->effectOffset);
                } else {
                    effectSpawnHit((u16)hitParameter, rootCoord, &scratch->effectOffset, &work->effectArg);
                }
            }
            hitParameter = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
            if (hitParameter > 0) {
                work->hitCooldown = hitParameter;
            }
            soundId = gGeneratorSoundIds[GENERATOR_BODY_HIT_SOUND_INDEX] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
    }
    // Discard contacts even while the hit cooldown blocks damage.
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(_GeneratorBodyHitScratch);
}
