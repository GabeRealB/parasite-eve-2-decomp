/* Part of the Glutton library; see glutton.h. */

/// Applies the first attack contact on groups 6, 7 and 8, in that order.
///
/// Damage uses player distance from a fixed point beside the host, is quadrupled
/// for an eligible critical hit, then divided by six with a minimum of one for
/// nonzero damage. It drains host HP and the groups' reaction pool and is
/// reported on escort 1. Critical hits and pool exhaustion request the summon
/// state, subject to the excluded states and player-catch gate.
/// The dumping hole requires a single combat reference, refills an exhausted
/// pool to 60, arms all hit cooldowns and mirrors HP onto escorts 0, 1 and 3.
/// The incinerator requires a nonzero fight phase, refills from escort 1's
/// maximum HP on either reaction, arms only this group's cooldown and reports
/// damage before subtracting host HP. Its critical effect reaches 600 local
/// units, versus 800 in the dumping hole, and the swipe preserves neck yaw.
/// Requires live host work, escorts 0, 1 and 3 and model coordinates, initialized
/// contacts and scratch capacity for `GluttonHitScratch` plus nested calls.
static void _gluttonHitGroups6To8(Task* task)
{
    enum { GLUTTON_GROUPS6_TO8_RANGE_X = 1311,
           GLUTTON_GROUPS6_TO8_RANGE_Y = 250,
           GLUTTON_GROUPS6_TO8_RANGE_Z = -607,
           GLUTTON_GROUPS6_TO8_POOL_HP = 60,
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
           GLUTTON_GROUPS6_TO8_CRITICAL_REACH = 800,
#else
           GLUTTON_GROUPS6_TO8_CRITICAL_REACH = 600,
#endif
    };
    GluttonHitScratch* scratch;
    GluttonWork*       work;
    Enemy*             host;
    PlayerStatus*      playerStatus;
    GfxCoord*          hitCoord;
    s32                attackKey;
    s32                playerDxSquared;
    s32                playerDySquared;
    s32                playerDzSquared;
    u32                reducedDamage;
    s16                contactYaw;
    s16                bossState;
    s16                hitCooldown;
    u16                hostHp;
    Enemy*             escort3;
    Enemy*             escort0;
    Enemy*             escort1;

    playerStatus       = &gPlayerStatus;
    host               = task->spawnArg2.pointer;
    work               = task->work;
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    attackKey          = _gluttonFindHit(&scratch->contactPoint, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts));
    scratch->attackKey = attackKey;
    // Contacts are consumed in group order; only one attack is applied.
    if (attackKey != 0) {
        hitCoord = work->hits[6].body.coord;
        goto hit;
    }
    attackKey          = _gluttonFindHit(&scratch->contactPoint, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts));
    scratch->attackKey = attackKey;
    if (attackKey != 0) {
        hitCoord = work->hits[7].body.coord;
    hit:
        _gluttonHitEffect(hitCoord, attackKey);
        if (scratch->attackKey != 0) {
            goto body;
        }
    }
    if (_gluttonScanGroup(scratch, &work->hits[8]) != 0 && _gluttonSpawnGroupHitEffect(scratch, &work->hits[8])) {
    body:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        hitCooldown              = damageGetPlayerAttackHitCooldown(scratch->attackKey);
        work->groups6To8Cooldown = hitCooldown;
        work->groups3To5Cooldown = hitCooldown;
        work->group0Cooldown     = hitCooldown;
        work->groups1To2Cooldown = hitCooldown;
#else
        work->groups6To8Cooldown = damageGetPlayerAttackHitCooldown(scratch->attackKey);
#endif
        damageGetPlayerAttackReaction(scratch->attackKey);

        // Range is measured from a fixed point in the host's parent frame.
        scratch->toPlayer.vx    = (playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0]) - GLUTTON_GROUPS6_TO8_RANGE_X;
        playerDxSquared         = scratch->toPlayer.vx * scratch->toPlayer.vx;
        scratch->toPlayer.vy    = (playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1]) - GLUTTON_GROUPS6_TO8_RANGE_Y;
        playerDySquared         = scratch->toPlayer.vy * scratch->toPlayer.vy;
        scratch->toPlayer.vz    = (playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2]) - GLUTTON_GROUPS6_TO8_RANGE_Z;
        playerDzSquared         = scratch->toPlayer.vz * scratch->toPlayer.vz;
        scratch->playerDistance = SquareRoot0(playerDxSquared + playerDySquared + playerDzSquared);
        scratch->damage         = damageComputePlayerAttack(scratch->attackKey, scratch->playerDistance, 0, 0);

        if (damageRollCriticalHit(work->escorts[1], scratch->attackKey, 0) != 0 && (bossState = work->state, bossState != GLUTTON_STATE_DEATH) && bossState != GLUTTON_STATE_INHALE &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON && bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->playerCaught != 1 &&
            gSceneCombatState.battleRefs == 1) {
#else
            bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON && bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->phase != 0 &&
            work->playerCaught != 1) {
            scratch->offset.vz = 0x3E8;
#endif
            scratch->offset.vy = 0;
            scratch->offset.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            scratch->offset.vz = GLUTTON_GROUPS6_TO8_CRITICAL_REACH;
#else
            scratch->offset.vy = 0;
            scratch->offset.vx = 0;
            scratch->offset.vz = GLUTTON_GROUPS6_TO8_CRITICAL_REACH;
#endif
            effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[1]->task->extra.tmd->coords[1], 0, &scratch->offset);
            scratch->damage *= 4;
            work->state      = GLUTTON_STATE_SUMMON;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
            work->groups6To8Pool = (s16)D_actor_444000_80144A48.hpMax;
#endif
        }

        // Keep every nonzero hit worth at least one HP after scaling.
        reducedDamage = scratch->damage / 6;
        if (reducedDamage == 0) {
            if (scratch->damage == 0) {
                scratch->damage = 0;
            } else {
                scratch->damage = 1;
            }
        } else {
            scratch->damage = reducedDamage;
        }
        damageAccumulateLifeDrainHp(host, scratch->attackKey, scratch->damage, 0);
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        worldTargetAddReadoutAmount(&work->escorts[1]->node, scratch->damage, 0);
#endif
        host->hp             -= scratch->damage;
        work->groups6To8Pool -= scratch->damage;
        if (work->groups6To8Pool <= 0 && (bossState = work->state, bossState != GLUTTON_STATE_DEATH) && bossState != GLUTTON_STATE_INHALE && bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->playerCaught != 1 && gSceneCombatState.battleRefs == 1) {
#else
            bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->phase != 0 && work->playerCaught != 1) {
            scratch->offset.vz = 0x3E8;
#endif
            scratch->offset.vy = 0;
            scratch->offset.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            scratch->offset.vz = GLUTTON_GROUPS6_TO8_CRITICAL_REACH;
#else
            scratch->offset.vy = 0;
            scratch->offset.vx = 0;
            scratch->offset.vz = GLUTTON_GROUPS6_TO8_CRITICAL_REACH;
#endif
            effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[1]->task->extra.tmd->coords[1], 0, &scratch->offset);
            work->state = GLUTTON_STATE_SUMMON;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            work->groups6To8Pool = GLUTTON_GROUPS6_TO8_POOL_HP;
#else
            work->groups6To8Pool = (s16)D_actor_444000_80144A48.hpMax;
#endif
        }

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        worldTargetAddReadoutAmount(&work->escorts[1]->node, scratch->damage, 0);
        escort3     = work->escorts[3];
        hostHp      = host->hp;
        escort0     = work->escorts[0];
        escort1     = work->escorts[1];
        escort3->hp = hostHp;
        escort1->hp = hostHp;
        escort0->hp = hostHp;
#endif
        // The readout is on escort 1; hit bearing uses escort 0's origin.
        work->escorts[1]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(work->escorts[1]->task->extra.tmd->coords);
        scratch->offset.vx = scratch->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
        scratch->offset.vy = scratch->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
        scratch->offset.vz = scratch->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
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
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}
