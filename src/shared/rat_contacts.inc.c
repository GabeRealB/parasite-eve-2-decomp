#include "gameplay/room_effects.h"

/* Part of the Rat library; see rat.h. */

/// Resolves the rat's grid/body contacts, player hits and bite sensor.
///
/// Requires live work, model root, enemy and player actor slots 0/1. Uses all
/// four grid records and three hit records, then clears each consumed table.
/// Grid corrections use Q16 world-unit components; opposed corrections restore
/// the previous root translation. The deepest player/enemy-body overlap moves
/// X/Z along a Q12 direction. Attack keys select player slot bit 7, damage and
/// reactions; fatal hits enter the death task state. A bite contact disables
/// pairing, and the one sensor record latches a player target and attack request.
static void _ratContacts(Task* actor)
{
    enum {
        RAT_CONTACT_KIND_SHIFT    = 16,
        RAT_CONTACT_KIND_NONE     = 0,
        RAT_PLAYER_REACTION_MASK  = 0xFFFF,
        RAT_PLAYER_SLOT_SHIFT     = 7,
        RAT_PLAYER_SLOT_MASK      = 1,
        RAT_GRID_FRACTION_BITS    = 16,
        RAT_CRITICAL_DAMAGE_SCALE = 4
    };
    // Retain the deepest positive body overlap and rotate its Q12 direction
    // out of the grid view. Captures rootCoord, scratch, deepestPush and the
    // delta/depth locals below; contact is a readable record expression,
    // evaluated repeatedly, so it must have no side effects.
#define RAT_KEEP_DEEPEST_BODY_PUSH(contact)                                                                                     \
    {                                                                                                                           \
        deltaX                   = rootCoord->workm.t[0] - (contact).point.vx;                                                  \
        scratch->delta.vector.vx = deltaX;                                                                                      \
        deltaY                   = rootCoord->workm.t[1] - (contact).point.vy;                                                  \
        scratch->delta.vector.vy = deltaY;                                                                                      \
        deltaZ                   = rootCoord->workm.t[2] - (contact).point.vz;                                                  \
        scratch->delta.vector.vz = deltaZ;                                                                                      \
        overlapDepth             = (contact).distance - SquareRoot0((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ)); \
        nonnegativeDepth         = overlapDepth;                                                                                \
        if (overlapDepth <= 0) {                                                                                                \
            nonnegativeDepth = 0;                                                                                               \
        }                                                                                                                       \
        overlapDepth = nonnegativeDepth;                                                                                        \
        if (deepestPush < overlapDepth) {                                                                                       \
            deepestPush = overlapDepth;                                                                                         \
            VectorNormal(&scratch->delta.vector, &scratch->normal);                                                             \
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);                \
        }                                                                                                                       \
    }

    RatWork*                 work;
    ActorOverlapPushScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                rootCoord;
    GfxCoord*                playerCoord;
    WorldCollisionContact*   attackContacts;
    WorldCollisionContact*   sensorContacts;
    s32                      deepestPush;
    s32                      gridResult;
    s32                      contactIndex;
    s32                      overlapDepth;
    s32                      deltaX;
    s32                      deltaY;
    s32                      deltaZ;
    s32                      nonnegativeDepth;
    s32                      hitCooldownFrames;
    u32                      lastEffectKey;
    u32                      contactKey;
    u32                      playerSlotBits;
    u32                      effectKey;
    u32                      hitDamage;

    deepestPush   = 0;
    lastEffectKey = 0;
    work          = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorOverlapPushScratch);
    scratch   = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    rootCoord = actor->extra.tmd->coords;
    enemy     = actor->spawnArg2.pointer;
    // Apply grid correction before hit damage and body-overlap selection.
    gridResult = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL);
    switch (gridResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.word >> RAT_GRID_FRACTION_BITS;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.word >> RAT_GRID_FRACTION_BITS;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.word >> RAT_GRID_FRACTION_BITS;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        contactKey = work->hitContacts[contactIndex].key.value;
        switch (contactKey >> RAT_CONTACT_KIND_SHIFT) {
            case RAT_CONTACT_KIND_NONE:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> RAT_CONTACT_KIND_SHIFT:
                if (work->hitCooldown == 0) {
                    playerSlotBits           = contactKey >> RAT_PLAYER_SLOT_SHIFT;
                    playerCoord              = gPlayerActorTasks[playerSlotBits & RAT_PLAYER_SLOT_MASK]->extra.tmd->coords;
                    scratch->delta.vector.vx = playerCoord->coord.t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vy = playerCoord->coord.t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vz = playerCoord->coord.t[2] - rootCoord->coord.t[2];
                    hitDamage                = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value, SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0) != 0) {
                        hitDamage *= RAT_CRITICAL_DAMAGE_SCALE;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    worldTargetAddReadoutAmount(&((Enemy*)actor->spawnArg2.pointer)->node, hitDamage, 0);
                    damageAccumulateLifeDrainHp(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, hitDamage, 0);
                    enemy->hp -= hitDamage;
                    if (enemy->hp <= 0) {
                        work->mode   = RAT_MODE_DEAD;
                        work->step   = 0;
                        actor->state = RAT_TASK_DEATH;
                    } else if (work->buildupHeld == 0) {
                        work->mode = RAT_MODE_HURT;
                        work->step = 0;
                    }
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & RAT_PLAYER_REACTION_MASK) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        case 4:
                        case 5:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        case 8:
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case 9:
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                    }
                    effectKey = work->hitContacts[contactIndex].key.value;
                    if (lastEffectKey != effectKey) {
                        lastEffectKey = effectKey;
                        effectSpawnHit(damageGetPlayerAttackEffectId(effectKey), rootCoord, NULL, &work->hitEffectArg);
                    }
                    hitCooldownFrames = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                    if (hitCooldownFrames > 0) {
                        work->hitCooldown = hitCooldownFrames;
                    }
                }
                break;
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> RAT_CONTACT_KIND_SHIFT:
                RAT_KEEP_DEEPEST_BODY_PUSH(work->hitContacts[contactIndex]);
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> RAT_CONTACT_KIND_SHIFT:
                RAT_KEEP_DEEPEST_BODY_PUSH(work->hitContacts[contactIndex]);
                break;
        }
    }
    if (deepestPush > 0) {
        rootCoord->coord.t[0] += (deepestPush * scratch->pushDirection.vx) >> RAT_DIRECTION_FRACTION_BITS;
        rootCoord->coord.t[2] += (deepestPush * scratch->pushDirection.vz) >> RAT_DIRECTION_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hitContacts);
    // The bite stops at its first contact; the sensor latches the next target.
    attackContacts = work->attackContacts;
    if (worldCollisionFindContactIndex(attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(attackContacts);
    }
    sensorContacts = work->sensorContacts;
    if (worldCollisionCountContactsByKind(sensorContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        playerCoord             = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> RAT_PLAYER_SLOT_SHIFT]->extra.tmd->coords;
        work->attackRequested   = 1;
        work->sensorBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->targetCoord       = playerCoord;
    }
    worldCollisionClearContacts(sensorContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);
#undef RAT_KEEP_DEEPEST_BODY_PUSH
}
