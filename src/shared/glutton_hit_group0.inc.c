/* Part of the Glutton library; see glutton.h. */

/// Applies one group-0 attack contact and the host's damage-over-time tick.
///
/// Attack damage is scaled by player distance, quadrupled on a critical hit,
/// then doubled before Life Drain credit, host HP subtraction and the readout.
/// The dumping-hole encounter arms all four hit cooldowns and mirrors HP onto
/// escorts 0, 1 and 3. The incinerator arms only group 0 and keeps the neck's
/// yaw during the swipe clip. Damage over time affects only host HP; only the
/// dumping-hole encounter also credits its nonzero ticks to Life Drain.
/// Requires live host work and model coordinates, initialized contacts and
/// scratch capacity for `GluttonHitScratch` plus nested damage/effect calls.
/// The dumping-hole instance additionally requires live escorts 0, 1 and 3.
static void _gluttonHitGroup0(Task* task)
{
    GluttonHitScratch* scratch;
    GluttonWork*       work;
    Enemy*             host;
    PlayerStatus*      playerStatus;
    s32                attackKey;
    s32                playerDxSquared;
    s32                playerDySquared;
    s32                playerDzSquared;
    s16                contactYaw;
    s16                hitCooldown;
    u16                hostHp;
    Enemy*             escort3;
    Enemy*             escort0;
    Enemy*             escort1;

    playerStatus = &gPlayerStatus;
    host         = task->spawnArg2.pointer;
    work         = task->work;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    attackKey    = _gluttonScanGroup(scratch, &work->hits[0]);

    // Contacts are consumed in group order; only one attack is applied.
    if (attackKey != 0) {
        _gluttonHitEffect(work->hits[0].body.coord, attackKey);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        hitCooldown              = damageGetPlayerAttackHitCooldown(scratch->attackKey);
        work->groups6To8Cooldown = hitCooldown;
        work->groups3To5Cooldown = hitCooldown;
        work->group0Cooldown     = hitCooldown;
        work->groups1To2Cooldown = hitCooldown;
#else
        work->group0Cooldown = damageGetPlayerAttackHitCooldown(scratch->attackKey);
#endif
        damageGetPlayerAttackReaction(scratch->attackKey);

        scratch->toPlayer.vx    = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
        playerDxSquared         = scratch->toPlayer.vx * scratch->toPlayer.vx;
        scratch->toPlayer.vy    = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
        playerDySquared         = scratch->toPlayer.vy * scratch->toPlayer.vy;
        scratch->toPlayer.vz    = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
        playerDzSquared         = scratch->toPlayer.vz * scratch->toPlayer.vz;
        scratch->playerDistance = SquareRoot0(playerDxSquared + playerDySquared + playerDzSquared);
        scratch->damage         = damageComputePlayerAttack(scratch->attackKey, scratch->playerDistance, 0, 0);
        if (damageRollCriticalHit(host, scratch->attackKey, 0) != 0) {
            scratch->damage *= 4;
        }
        if (scratch->damage != 0) {
            scratch->offset.vy = GLUTTON_GROUP0_HIT_FX_Y;
            scratch->offset.vx = 0;
            scratch->offset.vz = GLUTTON_GROUP0_HIT_FX_Z;
            effectSpawn(EFFECT_CRITICAL_HIT, &host->task->extra.tmd->coords[3], 3, &scratch->offset);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        scratch->offset.vx = task->extra.tmd->coords->workm.t[0];
        scratch->offset.vy = task->extra.tmd->coords->workm.t[1];
        scratch->offset.vz = task->extra.tmd->coords->workm.t[2];
#endif
        scratch->offset.vx = scratch->contactPoint.vx - task->extra.tmd->coords->workm.t[0];
        scratch->offset.vy = scratch->contactPoint.vy - task->extra.tmd->coords->workm.t[1];
        scratch->offset.vz = scratch->contactPoint.vz - task->extra.tmd->coords->workm.t[2];
        contactYaw         = ratan2(scratch->offset.vx, scratch->offset.vz) -
                     ratan2(-task->extra.tmd->coords->workm.m[2][0],
                            task->extra.tmd->coords->workm.m[2][2]);
        scratch->contactYaw = contactYaw;
        scratch->contactYaw = _actorAngleNormalizeYaw(contactYaw);

#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        if (work->animId != GLUTTON_ANIM_SWIPE) {
#endif
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        }
#endif
        scratch->damage *= 2;
        damageAccumulateLifeDrainHp(host, scratch->attackKey, scratch->damage, 0);
        host->hp -= scratch->damage;
        worldTargetAddReadoutAmount(&host->node, scratch->damage, 0);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        escort3     = work->escorts[3];
        hostHp      = host->hp;
        escort0     = work->escorts[0];
        escort1     = work->escorts[1];
        escort3->hp = hostHp;
        escort1->hp = hostHp;
        escort0->hp = hostHp;
#endif
    }

    if (host->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        scratch->damage = damageTickEnemyDamageOverTime(host);
        if (damageIsEnemyDamageOverTimeExpired(host) != 0) {
            host->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        if (scratch->damage != 0) {
            damageAccumulateLifeDrainHp(host, scratch->attackKey, scratch->damage, 0);
#endif
            host->hp -= scratch->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        }
#endif
    }

    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}
