/* Part of the Glutton library; see glutton.h. */

/// The hit handler for collision groups 6, 7 and 8 -- the same three-scan shape
/// as `func_actor_403200_8013A4A0` runs for groups 3, 4 and 5, with the next
/// group only scanned when the previous one landed nothing and the part it hit
/// reported no attack id back. In both room builds, the first two groups share one call site
/// through `coord`, and `Gp_GetIdParam0` is called and its kind thrown away.
///
/// Damage is the distance-scaled hit -- measured from an offset point rather
/// than the model origin -- quadrupled when `Gp_RollEnemyChance` fires, then
/// divided by six (never down to zero unless it already was), and comes off the
/// host, the two escorts sharing its pool and `groups6To8Pool`. Emptying that pool
/// spawns the same effect again and refills it to 0x3C. Both effect spawns and
/// the state change to 0xE are skipped while the boss is in one of the seven
/// states that ignore hits, while the player hold is armed, or while
/// `gSceneCombatState.battleRefs` is not 1.
///
/// The second escort carries the damage and the effect, but `sc->contactYaw` is the
/// yaw of the contact point relative to the first escort's facing. `esc3` /
/// `esc0` / `esc1` and the `hp` load sit after `func_800DA6E8`, unlike the
/// group 3-5 handler.
///
/// Groups 6 and 7 share one `gluttonHitEffect` call through `coord` and `id`,
/// which is the one jump left here: written as the `||` of three scan-and-land
/// pairs the group 3-5 handler uses, the three scans' match arms are laid out
/// elsewhere and the function is two instructions longer. Their scans call
/// `_gluttonFindHit` and store the key themselves; through `_gluttonScanGroup`
/// group 7's scan allocates its registers differently.
void gluttonHitGroups6To8(Task* arg0)
{
    GluttonHitScratch* sc;
    GluttonWork*       work;
    Enemy*             host;
    PlayerStatus*      cfg;
    GfxCoord*          coord;
    s32                id;
    s32                dx2;
    s32                dy2;
    s32                dz2;
    u32                dmg;
    s16                angle;
    s16                state;
    s16                param;
    u16                hp;
    Enemy*             esc3;
    Enemy*             esc0;
    Enemy*             esc1;

    cfg           = &gPlayerStatus;
    host          = (Enemy*)arg0->spawnArg2.pointer;
    work          = arg0->work;
    sc            = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    id            = _gluttonFindHit(&sc->contactPoint, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts));
    sc->attackKey = id;
    if (id != 0) {
        coord = work->hits[6].body.coord;
        goto hit;
    }
    id            = _gluttonFindHit(&sc->contactPoint, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts));
    sc->attackKey = id;
    if (id != 0) {
        coord = work->hits[7].body.coord;
    hit:
        gluttonHitEffect(coord, id);
        if (sc->attackKey != 0) {
            goto body;
        }
    }
    if (_gluttonScanGroup(sc, &work->hits[8]) != 0 && _gluttonHitLanded(sc, &work->hits[8])) {
    body:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        param                    = Gp_GetIdParam2(sc->attackKey);
        work->groups6To8Cooldown = param;
        work->groups3To5Cooldown = param;
        work->group0Cooldown     = param;
        work->groups1To2Cooldown = param;
#else
        work->groups6To8Cooldown = Gp_GetIdParam2(sc->attackKey);
#endif
        Gp_GetIdParam0(sc->attackKey);

        sc->toPlayer.vx    = (cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]) - 0x51F;
        dx2                = sc->toPlayer.vx * sc->toPlayer.vx;
        sc->toPlayer.vy    = (cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]) - 0xFA;
        dy2                = sc->toPlayer.vy * sc->toPlayer.vy;
        sc->toPlayer.vz    = (cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]) + 0x25F;
        dz2                = sc->toPlayer.vz * sc->toPlayer.vz;
        sc->playerDistance = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage         = Gp_ComputeDamage(sc->attackKey, sc->playerDistance, 0, 0);

        if (Gp_RollEnemyChance(work->escorts[1], sc->attackKey, 0) != 0 && (state = work->state, state != 0xD) && state != 3 &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->playerCaught != 1 &&
            gSceneCombatState.battleRefs == 1) {
#else
            state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->phase != 0 &&
            work->playerCaught != 1) {
            sc->offset.vz = 0x3E8;
#endif
            sc->offset.vy = 0;
            sc->offset.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            sc->offset.vz = 0x320;
#else
            sc->offset.vy = 0;
            sc->offset.vx = 0;
            sc->offset.vz = 0x258;
#endif
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->escorts[1]->task->extra.tmd->coords[1], 0, &sc->offset);
            sc->damage *= 4;
            work->state = 0xE;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
            work->groups6To8Pool = (s16)D_actor_444000_80144A48.hpMax;
#endif
        }

        dmg = sc->damage / 6;
        if (dmg == 0) {
            if (sc->damage == 0) {
                sc->damage = 0;
            } else {
                sc->damage = 1;
            }
        } else {
            sc->damage = dmg;
        }
        func_800E2C78(host, sc->attackKey, sc->damage, 0);
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        func_800DA6E8(&work->escorts[1]->node, sc->damage, 0);
#endif
        host->hp             -= sc->damage;
        work->groups6To8Pool -= sc->damage;
        if (work->groups6To8Pool <= 0 && (state = work->state, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            state != 0xF && state != 8 && state != 0xB && work->playerCaught != 1 && gSceneCombatState.battleRefs == 1) {
#else
            state != 0xF && state != 8 && state != 0xB && work->phase != 0 && work->playerCaught != 1) {
            sc->offset.vz = 0x3E8;
#endif
            sc->offset.vy = 0;
            sc->offset.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            sc->offset.vz = 0x320;
#else
            sc->offset.vy = 0;
            sc->offset.vx = 0;
            sc->offset.vz = 0x258;
#endif
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->escorts[1]->task->extra.tmd->coords[1], 0, &sc->offset);
            work->state = 0xE;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            work->groups6To8Pool = 0x3C;
#else
            work->groups6To8Pool = (s16)D_actor_444000_80144A48.hpMax;
#endif
        }

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        func_800DA6E8(&work->escorts[1]->node, sc->damage, 0);
        esc3     = work->escorts[3];
        hp       = host->hp;
        esc0     = work->escorts[0];
        esc1     = work->escorts[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
#endif
        work->escorts[1]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(work->escorts[1]->task->extra.tmd->coords);
        sc->offset.vx = sc->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
        sc->offset.vy = sc->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
        sc->offset.vz = sc->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
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
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}
