#include "gameplay/room_effects.h"

/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Per-frame collision pass: pushes the model out of what it touches, turns
/// it around on a blocking contact, and notes in `blocked` whether a body
/// or a wall blocked it.
void maggotCaterpillarResolveContacts(Task* arg0)
{
    MaggotCaterpillarWork*            work;
    MaggotCaterpillarContactsScratch* head;
    MaggotCaterpillarContactsScratch* scratch;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    GfxCoord*                         src;
    s32                               result;
    s32                               lastId;
    u32                               damage;
    s16                               amount;
    s32                               best;
    s32                               push;
    s32                               dx;
    s32                               dy;
    s32                               dz;
    VECTOR*                           normal;
    s32                               i;
    s32                               one;
    u32                               kind;

    best    = 0;
    lastId  = 0;
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    head    = SCRATCH_STACK_CURSOR(MaggotCaterpillarContactsScratch);
    scratch = SCRATCH_STACK_CURSOR(MaggotCaterpillarContactsScratch) = head - 1;
    enemy                                                            = (Enemy*)arg0->spawnArg2.pointer;
    work->landed                                                     = 0;
    result                                                           = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, 4, NULL);
    if (result != 0) {
        if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
            work->landed = 1;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->prevPos.vx;
                coord->coord.t[1] = work->prevPos.vy;
                coord->coord.t[2] = work->prevPos.vz;
                break;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    one = 1; // also stands for `MAGGOT_CATERPILLAR_REACTION_COMMITTED` and for the flags set below

    work->struck = 0;
    work->burst  = 0;
    normal       = &scratch->normal;
    for (i = 0; i < 2; i++) {
        kind = (u32)work->bodyContacts[i].key.value >> 16;
        if (kind == one)
            goto physical;
        if (kind == 0)
            goto next_contact;
        if (kind == 2)
            goto damage_contact;
        if (kind == 3)
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->hitCooldown == 0) {
            result = 0;
            if ((((u32)work->bodyContacts[i].key.value >> 8) & 0x3F) == 0x24) {
                if ((work->bodyContacts[i].key.value & 0x3F) == 0x24) {
                    result = 1;
                }
            }
            if ((result != one) || (work->burnFrame == 0)) {
                src                      = gPlayerActorTasks[((u32)work->bodyContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                dx                       = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = dx;
                dy                       = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = dy;
                dz                       = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = dz;
                damage                   = Gp_ComputeDamage((u32)work->bodyContacts[i].key.value, SquareRoot0(dx * dx + dy * dy + dz * dz), 0, 0);
                amount                   = damage;
                if (result == 0) {
                    if (work->midLeap != 0) {
                        amount = (damage << 16) >> 15;
                        effectSpawn(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 1, 3, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[i].key.value, 0) != 0) {
                        amount = (amount << 16) >> 14;
                        if (work->midLeap == 0) {
                            effectSpawn(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 1, 0, NULL);
                        }
                    }
                    damageAccumulateLifeDrainHp(enemy, work->bodyContacts[i].key.value, amount, 0);
                }
                worldTargetAddReadoutAmount(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if (work->reactionMode != one) {
                    if (enemy->hp <= 0) {
                        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                        work->step      = 0;
                        arg0->state     = 2;
                    } else if (result == 0) {
                        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_HURT;
                        work->step      = 0;
                    }
                }
                if (work->reactionMode == MAGGOT_CATERPILLAR_REACTION_REBOUND) {
                    if ((work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD) || (result == 0)) {
                        work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                        MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, &scratch->shortVector);
                    }
                }
                if (result == 0) {
                    work->struck            = one;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
                switch (damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->bodyContacts[i].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[i].key.value, 0);
                        break;
                    case 4:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        if (work->reactionMode != one) {
                            work->burst = one;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        if (work->burning == 0) {
                            work->burning          = one;
                            work->burnSoundTimer   = 0;
                            work->burnFrame        = 0;
                            work->flameBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                            worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_TINT);
                        }
                        break;
                }
                if (lastId != work->bodyContacts[i].key.value) {
                    lastId                  = work->bodyContacts[i].key.value;
                    scratch->shortVector.vx = 0;
                    scratch->shortVector.vy = -0xC8;
                    scratch->shortVector.vz = 0;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value), arg0->extra.tmd->coords + 1, &scratch->shortVector, &work->effectArg);
                }
                result = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
                if (result > 0) {
                    work->hitCooldown = result;
                }
            }
        }
        goto next_contact;
    physical:
        MAGGOT_CATERPILLAR_CONTACT_OVERLAP(push, coord, work->bodyContacts[i], scratch->delta);
        if (best < push) {
            best = push;
            MAGGOT_CATERPILLAR_GRID_DIRECTION(&scratch->delta, normal, &scratch->pushDirection);
        }
    next_contact:;
    }
    if (best > 0) {
        coord->coord.t[0] += (best * scratch->pushDirection.vx) >> 12;
        coord->coord.t[2] += (best * scratch->pushDirection.vz) >> 12;
    }
    worldCollisionClearContacts(work->bodyContacts);
    work->blocked = 0;
    if (work->attackContacts[0].flags & 1) {
        MAGGOT_CATERPILLAR_NOTE_BLOCKING_CONTACT(work, work->attackContacts[0]);
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        worldCollisionClearContacts(work->attackContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarContactsScratch);
}
