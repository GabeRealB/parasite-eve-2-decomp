/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Consumes grid, hit-body and attack contacts for one enemy frame.
///
/// Resolves four grid contacts as signed 16.16 room-space pushback, restoring
/// the prior root position for opposed normals. Scans both hit-body entries:
/// player attacks reduce halfword HP and may change behavior, while bodies
/// contribute only the deepest overlap's horizontal Q12-normal push.
/// Own flame contacts deal damage only at burn-cycle frame zero, bypass
/// critical/life-drain processing and do not set `struck`. Committed actions
/// retain behavior on damage; a rebound interrupted by a hit reverses yaw.
/// Requires valid attack-table keys and live player/companion task index 0/1.
/// Clears the consumed tables and releases its temporary scratch block.
static void _maggotCaterpillarResolveContacts(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_BURST_REACTION = 4 };
    MaggotCaterpillarWork*            work;
    MaggotCaterpillarContactsScratch* scratchEnd;
    MaggotCaterpillarContactsScratch* scratch;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    GfxCoord*                         attackerCoord;
    s32                               result;
    s32                               lastEffectKey;
    u32                               baseDamage;
    s16                               hitDamage;
    s32                               deepestOverlap;
    s32                               overlapDepth;
    s32                               attackerOffsetX;
    s32                               attackerOffsetY;
    s32                               attackerOffsetZ;
    VECTOR*                           pushNormal;
    s32                               contactIndex;
    s32                               one;
    u32                               contactKind;

    // Apply room-grid correction before measuring hit ranges and body overlap.
    deepestOverlap = 0;
    lastEffectKey  = 0;
    work           = actor->work;
    coord          = actor->extra.tmd->coords;
    scratchEnd     = SCRATCH_STACK_CURSOR(MaggotCaterpillarContactsScratch);
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(MaggotCaterpillarContactsScratch);
    enemy          = actor->spawnArg2.pointer;
    work->landed   = 0;
    result         = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL);
    if (result != 0) {
        if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
            work->landed = 1;
        }
        switch (result) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                coord->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
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
    one = MAGGOT_CATERPILLAR_REACTION_COMMITTED; // Shared value for committed reactions, category 1 and latched flags.

    work->struck = 0;
    work->burst  = 0;
    pushNormal   = &scratch->normal;
    for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        contactKind = (u32)work->bodyContacts[contactIndex].key.value >> 16;
        if (contactKind == one)
            goto physical;
        if (contactKind == 0)
            goto next_contact;
        if (contactKind == (WORLD_COLLISION_CONTACT_ATTACK >> 16))
            goto damage_contact;
        if (contactKind == (WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16))
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->hitCooldown == 0) {
            result = 0;
            if ((((u32)work->bodyContacts[contactIndex].key.value >> 8) & 0x3F) == MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW) {
                if ((work->bodyContacts[contactIndex].key.value & 0x3F) == MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW) {
                    result = 1;
                }
            }
            if ((result != one) || (work->burnFrame == 0)) {
                attackerCoord            = gPlayerActorTasks[((u32)work->bodyContacts[contactIndex].key.value >> 7) & 1]->extra.tmd->coords;
                attackerOffsetX          = attackerCoord->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = attackerOffsetX;
                attackerOffsetY          = attackerCoord->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = attackerOffsetY;
                attackerOffsetZ          = attackerCoord->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = attackerOffsetZ;
                baseDamage               = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, SquareRoot0(attackerOffsetX * attackerOffsetX + attackerOffsetY * attackerOffsetY + attackerOffsetZ * attackerOffsetZ), 0, 0);
                hitDamage                = baseDamage;
                if (result == 0) {
                    if (work->midLeap != 0) {
                        hitDamage = (baseDamage << 16) >> 15;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 3, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                        hitDamage = (hitDamage << 16) >> 14;
                        if (work->midLeap == 0) {
                            effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 0, NULL);
                        }
                    }
                    damageAccumulateLifeDrainHp(enemy, work->bodyContacts[contactIndex].key.value, hitDamage, 0);
                }
                // Preserve signed-halfword damage truncation and HP arithmetic.
                worldTargetAddReadoutAmount(&enemy->node, hitDamage, 0);
                enemy->hp -= hitDamage;
                if (work->reactionMode != one) {
                    if (enemy->hp <= 0) {
                        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                        work->step      = 0;
                        actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
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
                switch (damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        break;
                    case MAGGOT_CATERPILLAR_BURST_REACTION:
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
                            worldCoordSetActorColorMode(actor->spawnArg2.pointer, ENEMY_COLOR_TINT);
                        }
                        break;
                }
                if (lastEffectKey != work->bodyContacts[contactIndex].key.value) {
                    lastEffectKey           = work->bodyContacts[contactIndex].key.value;
                    scratch->shortVector.vx = 0;
                    scratch->shortVector.vy = -0xC8;
                    scratch->shortVector.vz = 0;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value), actor->extra.tmd->coords + 1, &scratch->shortVector, &work->effectArg);
                }
                result = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if (result > 0) {
                    work->hitCooldown = result;
                }
            }
        }
        goto next_contact;
    physical:
        MAGGOT_CATERPILLAR_CONTACT_OVERLAP(overlapDepth, coord, work->bodyContacts[contactIndex], scratch->delta);
        if (deepestOverlap < overlapDepth) {
            deepestOverlap = overlapDepth;
            MAGGOT_CATERPILLAR_GRID_DIRECTION(&scratch->delta, pushNormal, &scratch->pushDirection);
        }
    next_contact:;
    }
    if (deepestOverlap > 0) {
        coord->coord.t[0] += (deepestOverlap * scratch->pushDirection.vx) >> 12;
        coord->coord.t[2] += (deepestOverlap * scratch->pushDirection.vz) >> 12;
    }
    worldCollisionClearContacts(work->bodyContacts);
    work->blocked = 0;
    if (work->attackContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        MAGGOT_CATERPILLAR_NOTE_BLOCKING_CONTACT(work, work->attackContacts[0]);
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        worldCollisionClearContacts(work->attackContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarContactsScratch);
}
