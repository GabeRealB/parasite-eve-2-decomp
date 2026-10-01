/* Part of the Glutton library; see glutton.h. */

/// The group-0 hit handler: takes at most one hit this frame and turns it into
/// damage.
///
/// It carves a 0x30-byte `GluttonHitScratch` off the scratchpad stack and
/// scans the five `WorldCollisionContact` records of `hits[0]` for the first whose `key`
/// high halfword is attack kind 2 -- the contact point goes into the frame's
/// `pos` and the id is kept. A record with `key` 0 ends the scan with no
/// hit. The scan is written with labels rather than a `for` so `loop.c` parks
/// the match arm out of line; the same shape as
/// `func_actor_444000_8013C060`'s.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C, and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires. The contact point is re-read relative to the
/// part's world translation and `ratan2` of the pair against the part's facing
/// gives the yaw `angle`, wrapped to +/-0x800. The damage is doubled, applied
/// through `func_800E2C78` and `func_800DA6E8`, and the host's remaining HP is
/// mirrored onto the three escorts sharing its pool.
///
/// The second arm runs the same tick when `reactionFlags` has damage over time
/// set, which `Gp_TickObjFlag4` turns into damage of its own; that one only comes off the
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
    GluttonHitScratch*     sc;
    GluttonWork*           work;
    Enemy*                 enemy;
    WorldCollisionContact* recs;
    PlayerStatus*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg   = &gPlayerStatus;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (GluttonWork*)arg0->work;
    sc    = (GluttonHitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonHitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key.value == 0) {
        goto missed;
    }
    if ((recs[i].key.value & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key.value;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        gluttonHitEffect(work->hits[0].obj.coord, id);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
#else
        work->field_E8C = Gp_GetIdParam2(sc->id);
#endif
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            sc->rot.vy = 0x320;
#else
            sc->rot.vy = 0x190;
#endif
            sc->rot.vx = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            sc->rot.vz = 0x3E8;
#else
            sc->rot.vz = 0x1F4;
#endif
            Gp_SpawnEff(0x6009C, &enemy->task->extra.tmd->coords[3], 3, &sc->rot);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#else
        sc->rot.vx = arg0->extra.tmd->coords->workm.t[0];
        sc->rot.vy = arg0->extra.tmd->coords->workm.t[1];
        sc->rot.vz = arg0->extra.tmd->coords->workm.t[2];
#endif
        sc->rot.vx = sc->pos.vx - arg0->extra.tmd->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - arg0->extra.tmd->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - arg0->extra.tmd->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
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
        sc->damage *= 2;
        func_800E2C78(enemy, sc->id, sc->damage, 0);
        enemy->hp -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        esc3     = work->field_ECC[3];
        hp       = enemy->hp;
        esc0     = work->field_ECC[0];
        esc1     = work->field_ECC[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
#else
#endif
    }

    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        if (sc->damage != 0) {
            func_800E2C78(enemy, sc->id, sc->damage, 0);
#else
#endif
            enemy->hp -= sc->damage;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        }
#else
#endif
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonHitScratch));
}
