/* Part of the Glutton library; see glutton.h. */

/// Records the unwrapped contact yaw and returns it wrapped to a signed half turn.
///
/// Angles use 4096 units per turn; both half-turn endpoints are retained.
/// The scratch block is borrowed and must be writable.
static __inline__ s16 _gluttonRecordContactYaw(GluttonHitScratch* scratch, s16 contactYaw)
{
    scratch->contactYaw = contactYaw;
    if (contactYaw < 0) {
        while (1) {
            if (contactYaw >= -ACTOR_TRANSFORM_ANGLE_HALF_TURN)
                break;
            contactYaw += ACTOR_TRANSFORM_ANGLE_TURN;
        }
    } else {
        while (1) {
            if (contactYaw <= ACTOR_TRANSFORM_ANGLE_HALF_TURN)
                break;
            contactYaw -= ACTOR_TRANSFORM_ANGLE_TURN;
        }
    }
    return contactYaw;
}

/// Applies the first attack contact on group 1, falling back to group 2.
///
/// Distance-scaled damage is quadrupled for an eligible critical hit. Otherwise
/// only buildup attacks retain damage in the dumping hole; the incinerator
/// zeros it. Damage drains host HP and the groups' pool and is reported on
/// escort 3. The dumping hole arms all hit cooldowns and mirrors host HP to
/// escorts 0, 1 and 3; the incinerator arms only this pair's cooldown.
/// Buildup reactions request limb retraction directly; reactions 4 and 6 do so
/// with a one-in-six roll. Inhale excludes both reactions; the dumping hole
/// additionally requires no live summons, and the incinerator excludes the
/// arena-advance state. The incinerator preserves neck yaw during the swipe.
/// Requires live host work, escorts 0, 1 and 3 and model coordinates, initialized
/// contacts and scratch capacity for `GluttonHitScratch` plus nested calls.
static void _gluttonHitGroups1To2(Task* task)
{
    enum { GLUTTON_GROUPS1_TO2_CRITICAL_REACH = 1000 };
    GluttonHitScratch* scratch;
    GluttonWork*       work;
    Enemy*             host;
    PlayerStatus*      playerStatus;
    GfxCoord*          hitCoord;
    s32                attackKey;
    s32                playerDxSquared;
    s32                playerDySquared;
    s32                playerDzSquared;
    s16                contactYaw;
    s16                bossState;
    s16                hitCooldown;
    u16                reactionRoll;
    u16                hostHp;
    Enemy*             escort3;
    Enemy*             escort0;
    Enemy*             escort1;

    playerStatus       = &gPlayerStatus;
    host               = task->spawnArg2.pointer;
    work               = task->work;
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    attackKey          = _gluttonFindHit(&scratch->contactPoint, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts));
    scratch->attackKey = attackKey;
    // Contacts are consumed in group order; only one attack is applied.
    if (attackKey != 0) {
        hitCoord = work->hits[1].body.coord;
    } else {
        attackKey          = _gluttonFindHit(&scratch->contactPoint, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts));
        scratch->attackKey = attackKey;
        if (attackKey == 0) {
            SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
            return;
        }
        hitCoord = work->hits[2].body.coord;
    }
    _gluttonHitEffect(hitCoord, attackKey);
    if (scratch->attackKey != 0) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        hitCooldown              = damageGetPlayerAttackHitCooldown(scratch->attackKey);
        work->groups6To8Cooldown = hitCooldown;
        work->groups3To5Cooldown = hitCooldown;
        work->group0Cooldown     = hitCooldown;
        work->groups1To2Cooldown = hitCooldown;
#else
        work->groups1To2Cooldown = damageGetPlayerAttackHitCooldown(scratch->attackKey);
#endif
        switch (damageGetPlayerAttackReaction(scratch->attackKey) & 0xFFFF) {
            case DAMAGE_PLAYER_REACTION_NONE:
            case DAMAGE_PLAYER_REACTION_STAGGER:
            case DAMAGE_PLAYER_REACTION_POISON:
            case 5:
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
            case 8:
            case 9:
                break;

            case 4:
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
                bossState = work->state;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (bossState != GLUTTON_STATE_INHALE) {
#else
                if (bossState != GLUTTON_STATE_INHALE && bossState != GLUTTON_STATE_ADVANCE) {
#endif
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    reactionRoll    = (gRandomLcgState >> 16) % 6;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                    if (reactionRoll == 0 && work->summonsAlive == 0) {
#else
                    if (reactionRoll == 0) {
#endif
                        work->state     = GLUTTON_STATE_RETRACT_LIMB;
                        work->prevState = GLUTTON_STATE_REENTER;
                    }
                }
                break;

            case DAMAGE_PLAYER_REACTION_BUILDUP:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (work->summonsAlive == 0 && (bossState = work->state, bossState != GLUTTON_STATE_INHALE)) {
#else
                bossState = work->state;
                if (bossState != GLUTTON_STATE_INHALE && bossState != GLUTTON_STATE_ADVANCE) {
#endif
                    work->state     = GLUTTON_STATE_RETRACT_LIMB;
                    work->prevState = GLUTTON_STATE_REENTER;
                }
                break;
        }

        scratch->toPlayer.vx    = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
        playerDxSquared         = scratch->toPlayer.vx * scratch->toPlayer.vx;
        scratch->toPlayer.vy    = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
        playerDySquared         = scratch->toPlayer.vy * scratch->toPlayer.vy;
        scratch->toPlayer.vz    = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
        playerDzSquared         = scratch->toPlayer.vz * scratch->toPlayer.vz;
        scratch->playerDistance = SquareRoot0(playerDxSquared + playerDySquared + playerDzSquared);
        scratch->damage         = damageComputePlayerAttack(scratch->attackKey, scratch->playerDistance, 0, 0);

        if (damageRollCriticalHit(work->escorts[3], scratch->attackKey, 0) != 0 && (bossState = work->state, bossState != GLUTTON_STATE_DEATH) && bossState != GLUTTON_STATE_INHALE &&
            bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON && bossState != GLUTTON_STATE_HEAL) {
            scratch->offset.vy = 0;
            scratch->offset.vx = 0;
            scratch->offset.vz = GLUTTON_GROUPS1_TO2_CRITICAL_REACH;
            effectSpawn(EFFECT_CRITICAL_HIT, work->escorts[3]->task->extra.tmd->coords, 0, &scratch->offset);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            if (work->state != GLUTTON_STATE_ADVANCE && work->summonsAlive == 0) {
#else
            if (work->state != GLUTTON_STATE_ADVANCE) {
#endif
                work->state     = GLUTTON_STATE_RETRACT_LIMB;
                work->prevState = GLUTTON_STATE_REENTER;
            }
            scratch->damage *= 4;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        } else if ((damageGetPlayerAttackReaction(scratch->attackKey) & 0xFFFF) != DAMAGE_PLAYER_REACTION_BUILDUP) {
#else
        } else {
#endif
            scratch->damage = 0;
        }

        // Apply this target's damage before recording the contact bearing.
        damageAccumulateLifeDrainHp(host, scratch->attackKey, scratch->damage, 0);
        host->hp -= scratch->damage;
        worldTargetAddReadoutAmount(&work->escorts[3]->node, scratch->damage, 0);
        work->groups1To2Pool -= scratch->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        escort3     = work->escorts[3];
        hostHp      = host->hp;
        escort0     = work->escorts[0];
        escort1     = work->escorts[1];
        escort3->hp = hostHp;
        escort1->hp = hostHp;
        escort0->hp = hostHp;
#endif
        work->escorts[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(work->escorts[3]->task->extra.tmd->coords);
        scratch->offset.vx = scratch->contactPoint.vx - work->escorts[3]->task->extra.tmd->coords->workm.t[0];
        scratch->offset.vy = scratch->contactPoint.vy - work->escorts[3]->task->extra.tmd->coords->workm.t[1];
        scratch->offset.vz = scratch->contactPoint.vz - work->escorts[3]->task->extra.tmd->coords->workm.t[2];
        contactYaw         = ratan2(scratch->offset.vx, scratch->offset.vz) -
                     ratan2(-task->extra.tmd->coords->workm.m[2][0],
                            task->extra.tmd->coords->workm.m[2][2]);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        do {
#endif
            contactYaw = _gluttonRecordContactYaw(scratch, contactYaw);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        } while (0);
#endif
        scratch->contactYaw = contactYaw;

#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        if (work->animId != GLUTTON_ANIM_SWIPE) {
#endif
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        }
#endif
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}
