#include "gameplay/room_effects.h"

/* Part of the Rat library; see rat.h. */

/// Per-frame collision pass: applies the pending move to the root coordinate,
/// then walks the three contact records - kind 2 is a hit that damages the
/// enemy, kinds 1 and 3 an obstacle to push out of - and applies the deepest
/// push at the end.
void ratContacts(Task* actor)
{
    RatWork*                 work;
    ActorOverlapPushScratch* frame;
    Enemy*                   ctx;
    GfxCoord*                coord;
    GfxCoord*                sourceCoord;
    WorldCollisionContact*   effectRec;
    WorldCollisionContact*   contactRec;
    s32                      push;
    s32                      result;
    s32                      i;
    s32                      depth;
    s32                      x;
    s32                      y;
    s32                      z;
    s32                      boundedDepth;
    s32                      cooldownParam;
    u32                      lastId;
    u32                      id;
    u32                      slot;
    u32                      hitId;
    u32                      damage;

    push   = 0;
    lastId = 0;
    work   = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorOverlapPushScratch);
    frame  = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    coord  = actor->extra.tmd->coords;
    ctx    = actor->spawnArg2.pointer;
    result = worldCollisionResolvePushback(work->gridContacts, &frame->delta, 4, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += frame->delta.fixed.vx.word >> 16;
            coord->coord.t[1] += frame->delta.fixed.vy.word >> 16;
            coord->coord.t[2] += frame->delta.fixed.vz.word >> 16;
            break;
        case 2:
            coord->coord.t[0] = work->prevPos.vx;
            coord->coord.t[1] = work->prevPos.vy;
            coord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        id = work->hitContacts[i].key.value;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->hitCooldown == 0) {
                    slot                   = id >> 7;
                    sourceCoord            = gPlayerActorTasks[slot & 1]->extra.tmd->coords;
                    frame->delta.vector.vx = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vy = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vz = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage                 = damageComputePlayerAttack(work->hitContacts[i].key.value, SquareRoot0((frame->delta.vector.vx * frame->delta.vector.vx) + (frame->delta.vector.vy * frame->delta.vector.vy) + (frame->delta.vector.vz * frame->delta.vector.vz)), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->hitContacts[i].key.value, 0) != 0) {
                        damage *= 4;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    worldTargetAddReadoutAmount(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    damageAccumulateLifeDrainHp(actor->spawnArg2.pointer, work->hitContacts[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->mode   = RAT_MODE_DEAD;
                        work->step   = 0;
                        actor->state = 2;
                    } else if (work->buildupHeld == 0) {
                        work->mode = RAT_MODE_HURT;
                        work->step = 0;
                    }
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (damageGetPlayerAttackReaction(work->hitContacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        case 4:
                        case 5:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        case 8:
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(actor->spawnArg2.pointer, work->hitContacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(actor->spawnArg2.pointer, work->hitContacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case 9:
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                    }
                    hitId = work->hitContacts[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        effectSpawnHit(damageGetPlayerAttackEffectId(hitId), coord, NULL, &work->hitEffectArg);
                    }
                    cooldownParam = damageGetPlayerAttackHitCooldown(work->hitContacts[i].key.value);
                    if (cooldownParam > 0) {
                        work->hitCooldown = cooldownParam;
                    }
                }
                break;
            case 1:
                x                      = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vx = x;
                y                      = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vy = y;
                z                      = coord->workm.t[2] - work->hitContacts[i].point.vz;
                frame->delta.vector.vz = z;
                depth                  = work->hitContacts[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal(&frame->delta.vector, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->pushDirection);
                }
                break;
            case 3:
                x                      = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vx = x;
                y                      = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vy = y;
                z                      = coord->workm.t[2] - work->hitContacts[i].point.vz;
                frame->delta.vector.vz = z;
                depth                  = work->hitContacts[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal(&frame->delta.vector, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->pushDirection);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (push * frame->pushDirection.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->pushDirection.vz) >> 0xC;
    }
    worldCollisionClearContacts(work->hitContacts);
    effectRec = work->attackContacts;
    if (worldCollisionFindContactIndex(effectRec, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(effectRec);
    }
    contactRec = work->sensorContacts;
    if (worldCollisionCountContactsByKind(contactRec, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        sourceCoord             = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> 7]->extra.tmd->coords;
        work->attackRequested   = 1;
        work->sensorBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->targetCoord       = sourceCoord;
    }
    worldCollisionClearContacts(contactRec);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);
}
