/* Part of the Glutton library; see glutton.h. */

/// The group-0 hit handler: takes at most one hit this frame and turns it into
/// damage.
///
/// It carves a 0x30-byte `GluttonHitScratch` off the scratchpad stack and
/// scans the five `WorldCollisionContact` records of `hits[0]` for the first whose `key`
/// high halfword is attack kind 2 -- the contact point goes into the block's
/// `contactPoint` and the key into `attackKey`. A record with `key` 0 ends the scan with no
/// hit. The scan is `_gluttonScanGroup`.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `damageGetPlayerAttackHitCooldown` of the attack id to all four per-group slots at 0xE8C, and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the `playerDistance` `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `damageRollCriticalHit` fires. The contact point is re-read relative to the
/// part's world translation and `ratan2` of the pair against the part's facing
/// gives the yaw `contactYaw`, wrapped to +/-0x800. The damage is doubled,
/// credits Life Drain healing through `damageAccumulateLifeDrainHp`, updates
/// the readout through `worldTargetAddReadoutAmount`, and is taken from the host's HP, which is
/// mirrored onto the three escorts sharing its pool.
///
/// The second arm runs the same tick when `reactionFlags` has damage over time
/// set, which `damageTickEnemyDamageOverTime` turns into damage of its own; that one only comes off the
/// host.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads (the scheduler cannot hoist a load
/// past a store through an unknown pointer). Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
void gluttonHitGroup0(Task* arg0)
{
    GluttonHitScratch* sc;
    GluttonWork*       work;
    Enemy*             enemy;
    PlayerStatus*      cfg;
    s32                id;
    s32                dx2;
    s32                dy2;
    s32                dz2;
    s16                angle;
    s16                param;
    u16                hp;
    Enemy*             esc3;
    Enemy*             esc0;
    Enemy*             esc1;

    cfg   = &gPlayerStatus;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    sc    = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    id    = _gluttonScanGroup(sc, &work->hits[0]);

    if (id != 0) {
        gluttonHitEffect(work->hits[0].body.coord, id);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        param                    = damageGetPlayerAttackHitCooldown(sc->attackKey);
        work->groups6To8Cooldown = param;
        work->groups3To5Cooldown = param;
        work->group0Cooldown     = param;
        work->groups1To2Cooldown = param;
#else
        work->group0Cooldown = damageGetPlayerAttackHitCooldown(sc->attackKey);
#endif
        damageGetPlayerAttackReaction(sc->attackKey);

        sc->toPlayer.vx    = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2                = sc->toPlayer.vx * sc->toPlayer.vx;
        sc->toPlayer.vy    = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2                = sc->toPlayer.vy * sc->toPlayer.vy;
        sc->toPlayer.vz    = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2                = sc->toPlayer.vz * sc->toPlayer.vz;
        sc->playerDistance = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage         = Gp_ComputeDamage(sc->attackKey, sc->playerDistance, 0, 0);
        if (damageRollCriticalHit(enemy, sc->attackKey, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->offset.vy = GLUTTON_GROUP0_HIT_FX_Y;
            sc->offset.vx = 0;
            sc->offset.vz = GLUTTON_GROUP0_HIT_FX_Z;
            effectSpawn(EFFECT_CRITICAL_HIT, &enemy->task->extra.tmd->coords[3], 3, &sc->offset);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg0->extra.tmd->coords);
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        sc->offset.vx = arg0->extra.tmd->coords->workm.t[0];
        sc->offset.vy = arg0->extra.tmd->coords->workm.t[1];
        sc->offset.vz = arg0->extra.tmd->coords->workm.t[2];
#endif
        sc->offset.vx = sc->contactPoint.vx - arg0->extra.tmd->coords->workm.t[0];
        sc->offset.vy = sc->contactPoint.vy - arg0->extra.tmd->coords->workm.t[1];
        sc->offset.vz = sc->contactPoint.vz - arg0->extra.tmd->coords->workm.t[2];
        angle         = ratan2(sc->offset.vx, sc->offset.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
        sc->contactYaw = angle;
        sc->contactYaw = actorWrapAngle(angle);

#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        if (work->animId != 4) {
#endif
            work->neckYaw       = 0;
            work->neckYawTarget = 0;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        }
#endif
        sc->damage *= 2;
        damageAccumulateLifeDrainHp(enemy, sc->attackKey, sc->damage, 0);
        enemy->hp -= sc->damage;
        worldTargetAddReadoutAmount(&enemy->node, sc->damage, 0);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        esc3     = work->escorts[3];
        hp       = enemy->hp;
        esc0     = work->escorts[0];
        esc1     = work->escorts[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
#endif
    }

    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        sc->damage = damageTickEnemyDamageOverTime(enemy);
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        if (sc->damage != 0) {
            damageAccumulateLifeDrainHp(enemy, sc->attackKey, sc->damage, 0);
#endif
            enemy->hp -= sc->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        }
#endif
    }

    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}
