/* Part of the Glutton library; see glutton.h. */

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `gluttonHitGroup0` runs for group 0, done twice: group 1 first, and
/// group 2 only if nothing landed on group 1. The second scan carries its own
/// `recs2` / `pos2` / `i2`, because sharing `recs` / `pos` / `i` with the first
/// gives both loops one pseudo each and the wrong registers. Both scans are
/// written as real `for` loops rather than the group-0 handler's labels so
/// `find_and_verify_loops` parks the match arm out of line.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires, and zeroed unless the attack kind came back 2.
/// The damage also comes off the work block's `field_F0E` pool and the host's
/// remaining HP is mirrored onto the three escorts sharing its pool.
/// `sc->angle` is the yaw of the contact point relative to the fourth escort's
/// facing, wrapped to +/-0x800.
///
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `gRandomLcgState`
/// and take the boss out of state 3 into 8 one time in six, kind 2 does it
/// outright, and both are gated on the `field_F1C` re-arm countdown.
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
    GluttonHitScratch*     sc;
    GluttonWork*           work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    s16 param;
#else
#endif
    u16 roll;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    u16    hp;
    Enemy* esc3;
    Enemy* esc0;
    Enemy* esc1;
#else
#endif

    cfg = &gPlayerStatus;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = (GluttonWork*)arg0->work;
#else
    host = arg0->spawnArg2.pointer;
    work = arg0->work;
#endif
    sc   = (GluttonHitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonHitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key.value == 0) {
            goto missed1;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key.value;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key.value == 0) {
            goto missed2;
        }
        if ((recs2[i2].key.value & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key.value;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    coord = work->hits[2].obj.coord;
hit:
    gluttonHitEffect(coord, id);
    if (sc->id != 0) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
#else
        work->field_E92 = Gp_GetIdParam2(sc->id);
#endif
        switch (Gp_GetIdParam0(sc->id) & 0xFFFF) {
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
                state = work->field_0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (state != 3) {
#else
                if (state != 3 && state != 9) {
#endif
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    roll            = (gRandomLcgState >> 16) % 6;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                    if (roll == 0 && work->field_F1C == 0) {
#else
                    if (roll == 0) {
#endif
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                if (work->field_F1C == 0 && (state = work->field_0, state != 3)) {
#else
                state = work->field_0;
                if (state != 3 && state != 9) {
#endif
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, work->field_ECC[3]->task->extra.tmd->coords, 0, &sc->rot);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            if (work->field_0 != 9 && work->field_F1C == 0) {
#else
            if (work->field_0 != 9) {
#endif
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        } else if ((Gp_GetIdParam0(sc->id) & 0xFFFF) != 2) {
#else
        } else {
#endif
            sc->damage = 0;
        }

        func_800E2C78(host, sc->id, sc->damage, 0);
        host->hp -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E -= sc->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        esc3     = work->field_ECC[3];
        hp       = host->hp;
        esc0     = work->field_ECC[0];
        esc1     = work->field_ECC[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
#else
#endif
        work->field_ECC[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(work->field_ECC[3]->task->extra.tmd->coords);
        sc->rot.vx = sc->pos.vx - work->field_ECC[3]->task->extra.tmd->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - work->field_ECC[3]->task->extra.tmd->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - work->field_ECC[3]->task->extra.tmd->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        do {
#else
#endif
            sc->angle = angle;
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
#else
#endif
        sc->angle = angle;

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#else
        if (work->field_7B3 != 4) {
#endif
        work->field_7C8 = 0;
        work->field_7C4 = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#else
        }
#endif
    }
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonHitScratch));
}
