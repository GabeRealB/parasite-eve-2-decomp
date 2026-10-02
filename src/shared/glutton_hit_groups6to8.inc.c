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
/// host, the two escorts sharing its pool and `field_F0C`. Emptying that pool
/// spawns the same effect again and refills it to 0x3C. Both effect spawns and
/// the state change to 0xE are skipped while the boss is in one of the seven
/// states that ignore hits, while the player hold is armed, or while
/// `gSceneCombatState.battleRefs` is not 1.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the first escort's facing. `pos` /
/// `pos2` / `pos3` are all `&sc->pos` and are not spare: each group's scan
/// writes the contact point through its own pointer. `esc3` / `esc0` / `esc1`
/// and the `hp` load sit after `func_800DA6E8`, unlike the group 3-5 handler.
void gluttonHitGroups6To8(Task* arg0)
{
    GluttonHitScratch*     sc;
    GluttonWork*           work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    WorldCollisionContact* recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg  = &gPlayerStatus;
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = (GluttonWork*)arg0->work;
    sc   = (GluttonHitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonHitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
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
        coord = work->hits[6].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[7].recs;
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
    if (id != 0) {
        coord = work->hits[7].obj.coord;
    hit:
        gluttonHitEffect(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key.value == 0) {
            goto missed3;
        }
        if ((recs3[i3].key.value & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key.value;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    gluttonHitEffect(work->hits[8].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
#else
    work->field_E90 = Gp_GetIdParam2(sc->id);
#endif
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]) - 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[1], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        gSceneCombatState.battleRefs == 1) {
#else
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
#endif
        sc->rot.vy = 0;
        sc->rot.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        sc->rot.vz = 0x320;
#else
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
#endif
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->field_ECC[1]->task->extra.tmd->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
        work->field_F0C = (s16)D_actor_444000_80144A48.hpMax;
#endif
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->id, sc->damage, 0);
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
#endif
    host->hp        -= sc->damage;
    work->field_F0C -= sc->damage;
    if (work->field_F0C <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && gSceneCombatState.battleRefs == 1) {
#else
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
#endif
        sc->rot.vy = 0;
        sc->rot.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        sc->rot.vz = 0x320;
#else
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
#endif
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->field_ECC[1]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0 = 0xE;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        work->field_F0C = 0x3C;
#else
        work->field_F0C = (s16)D_actor_444000_80144A48.hpMax;
#endif
    }

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    esc3     = work->field_ECC[3];
    hp       = host->hp;
    esc0     = work->field_ECC[0];
    esc1     = work->field_ECC[1];
    esc3->hp = hp;
    esc1->hp = hp;
    esc0->hp = hp;
#endif
    work->field_ECC[1]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(work->field_ECC[1]->task->extra.tmd->coords);
    sc->rot.vx = sc->pos.vx - work->field_ECC[0]->task->extra.tmd->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - work->field_ECC[0]->task->extra.tmd->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - work->field_ECC[0]->task->extra.tmd->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                   arg0->extra.tmd->coords->workm.m[2][2]);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    do {
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
#endif
    sc->angle = angle;

#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    if (work->field_7B3 != 4) {
#endif
        work->field_7C8 = 0;
        work->field_7C4 = 0;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    }
#endif
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonHitScratch));
}
