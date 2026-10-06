/* Part of the Glutton library; see glutton.h. */

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `gluttonHitGroup0` runs for group 0, done twice: group 1 first, and
/// group 2 only if nothing landed on group 1. Each scan is `_gluttonFindHit`
/// with the key stored here; through `_gluttonScanGroup` the second scan
/// allocates its registers differently, as in `gluttonHitGroups6To8`.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the `playerDistance` `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires, and zeroed unless the attack kind came back 2.
/// The damage also comes off the work block's `groups1To2Pool` pool and the host's
/// remaining HP is mirrored onto the three escorts sharing its pool.
/// `sc->contactYaw` is the yaw of the contact point relative to the fourth escort's
/// facing, wrapped to +/-0x800.
///
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `gRandomLcgState`
/// and take the boss out of state 3 into 8 one time in six, kind 2 does it
/// outright, and both are gated on the `summonsAlive` re-arm countdown.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads. Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
///
/// The `do` / `while (0)` around the angle wrap is load-bearing, not stylistic.
/// Its body sits at loop depth 1, so `flow.c`'s `REG_N_REFS (regno) +=
/// loop_depth` gives `sc` one reference more than the unwrapped form (37
/// against 36, `work` sitting at 37 on a longer live range). That is what ranks
/// `sc` above `work` in global-alloc and puts it in `$s1`; unwrapped the two
/// exchange registers and the function stops at 99.06%.
void gluttonHitGroups1To2(Task* arg0)
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
    s16                angle;
    s16                state;
    s16                param;
    u16                roll;
    u16                hp;
    Enemy*             esc3;
    Enemy*             esc0;
    Enemy*             esc1;

    cfg           = &gPlayerStatus;
    host          = (Enemy*)arg0->spawnArg2.pointer;
    work          = arg0->work;
    sc            = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    id            = _gluttonFindHit(&sc->contactPoint, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts));
    sc->attackKey = id;
    if (id != 0) {
        coord = work->hits[1].body.coord;
    } else {
        id            = _gluttonFindHit(&sc->contactPoint, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts));
        sc->attackKey = id;
        if (id == 0) {
            SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
            return;
        }
        coord = work->hits[2].body.coord;
    }
    gluttonHitEffect(coord, id);
    if (sc->attackKey != 0) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        param                    = Gp_GetIdParam2(sc->attackKey);
        work->groups6To8Cooldown = param;
        work->groups3To5Cooldown = param;
        work->group0Cooldown     = param;
        work->groups1To2Cooldown = param;
#else
        work->groups1To2Cooldown = Gp_GetIdParam2(sc->attackKey);
#endif
        switch (Gp_GetIdParam0(sc->attackKey) & 0xFFFF) {
            case 0:
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 9:
                break;

            case 4:
            case 6:
                state = work->state;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (state != 3) {
#else
                if (state != 3 && state != 9) {
#endif
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    roll            = (gRandomLcgState >> 16) % 6;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                    if (roll == 0 && work->summonsAlive == 0) {
#else
                    if (roll == 0) {
#endif
                        work->state     = 8;
                        work->prevState = -1;
                    }
                }
                break;

            case 2:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (work->summonsAlive == 0 && (state = work->state, state != 3)) {
#else
                state = work->state;
                if (state != 3 && state != 9) {
#endif
                    work->state     = 8;
                    work->prevState = -1;
                }
                break;
        }

        sc->toPlayer.vx    = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2                = sc->toPlayer.vx * sc->toPlayer.vx;
        sc->toPlayer.vy    = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2                = sc->toPlayer.vy * sc->toPlayer.vy;
        sc->toPlayer.vz    = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2                = sc->toPlayer.vz * sc->toPlayer.vz;
        sc->playerDistance = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage         = Gp_ComputeDamage(sc->attackKey, sc->playerDistance, 0, 0);

        if (Gp_RollEnemyChance(work->escorts[3], sc->attackKey, 0) != 0 && (state = work->state, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->offset.vy = 0;
            sc->offset.vx = 0;
            sc->offset.vz = 0x3E8;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, work->escorts[3]->task->extra.tmd->coords, 0, &sc->offset);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            if (work->state != 9 && work->summonsAlive == 0) {
#else
            if (work->state != 9) {
#endif
                work->state     = 8;
                work->prevState = -1;
            }
            sc->damage *= 4;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        } else if ((Gp_GetIdParam0(sc->attackKey) & 0xFFFF) != 2) {
#else
        } else {
#endif
            sc->damage = 0;
        }

        func_800E2C78(host, sc->attackKey, sc->damage, 0);
        host->hp -= sc->damage;
        func_800DA6E8(&work->escorts[3]->node, sc->damage, 0);
        work->groups1To2Pool -= sc->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        esc3     = work->escorts[3];
        hp       = host->hp;
        esc0     = work->escorts[0];
        esc1     = work->escorts[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
#endif
        work->escorts[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(work->escorts[3]->task->extra.tmd->coords);
        sc->offset.vx = sc->contactPoint.vx - work->escorts[3]->task->extra.tmd->coords->workm.t[0];
        sc->offset.vy = sc->contactPoint.vy - work->escorts[3]->task->extra.tmd->coords->workm.t[1];
        sc->offset.vz = sc->contactPoint.vz - work->escorts[3]->task->extra.tmd->coords->workm.t[2];
        angle         = ratan2(sc->offset.vx, sc->offset.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        do {
#endif
            sc->contactYaw = angle;
            if (angle < 0) {
            wrapUp:
                if (angle < -0x800) {
                    angle += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (angle > 0x800) {
                    angle -= 0x1000;
                    goto wrapDown;
                }
            }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        } while (0);
#endif
        sc->contactYaw = angle;

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
