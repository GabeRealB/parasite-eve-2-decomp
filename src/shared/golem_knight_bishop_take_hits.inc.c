/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Per-frame hit handler: applies the `func_800E0C10` push-back from the
/// `field_504` and (while `field_49A` enables grid tests) `field_49C`
/// record tables to the root coordinate, ticks the `field_6C6` flinch
/// countdown, and for each kind-2 hit record in `field_49C` computes the
/// damage from the distance to the player, applies it to the `Enemy`,
/// spawns the hit sparks once per distinct id and hands the damage to
/// `golemKnightBishopPickHitReaction` unless the vocal cue is armed.
void golemKnightBishopTakeHits(Task* arg0)
{
    s32                          lastId;
    GolemKnightBishopWork*       work;
    GolemKnightBishopHitScratch* head;
    GolemKnightBishopHitScratch* sc;
    GolemKnightBishopHitScratch* blk;
    Enemy*                       enemy;
    GfxCoord*                    coord;
    s32                          i;
    s32                          damage;
    s32                          kind;
    s32                          wait;
    s16                          t;

    lastId                                            = 0;
    work                                              = arg0->work;
    head                                              = SCRATCH_STACK_CURSOR(GolemKnightBishopHitScratch);
    blk                                               = head - 1;
    SCRATCH_STACK_CURSOR(GolemKnightBishopHitScratch) = blk;
    sc                                                = blk;
    coord                                             = arg0->extra.tmd->coords;
    enemy                                             = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_504, &sc->delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.vx.halves.integer;
            coord->coord.t[1] += sc->delta.vy.halves.integer;
            coord->coord.t[2] += sc->delta.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_664;
            coord->coord.t[1] = work->field_668;
            coord->coord.t[2] = work->field_66C;
            break;
    }
    Gp_ClearRec18Occupied(work->field_504);

    if (work->field_49A & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->field_49C, &sc->delta, 3, NULL)) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += sc->delta.vx.halves.integer;
                coord->coord.t[2] += sc->delta.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->field_664;
                coord->coord.t[2] = work->field_66C;
                break;
        }
    }

    if (work->field_6C6 != 0) {
        t               = work->field_6C6 - 1;
        work->field_6C6 = t;
        if (t <= 0) {
            work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_6C6  = 0;
            work->field_494  = work->field_716 | 0x30000;
        }
    }

    for (i = 0; i < 3; i++) {
        switch ((u32)work->field_49C[i].key.value >> 16) {
            case 0:
                break;
            case 1:
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                }
                break;
            case 2:
                if (work->field_6C6 != 0) {
                    break;
                }
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                    break;
                }
                sc->delta.vx.word = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.word = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.word = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->field_6D2   = (u32) ~(sc->delta.vx.word * coord->coord.m[0][2] +
                                          sc->delta.vy.word * coord->coord.m[1][2] +
                                          sc->delta.vz.word * coord->coord.m[2][2]) >>
                                  31;
                damage = Gp_ComputeDamage(work->field_49C[i].key.value,
                                          SquareRoot0(sc->delta.vx.word * sc->delta.vx.word +
                                                      sc->delta.vy.word * sc->delta.vy.word +
                                                      sc->delta.vz.word * sc->delta.vz.word),
                                          0, 0);
                kind   = Gp_GetIdParam0(work->field_49C[i].key.value);
                if ((u16)kind == 5) {
                    damage *= 2;
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if (Gp_RollEnemyChance(enemy, work->field_49C[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((u16)kind != 5) {
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_49C[i].key.value, damage, 0);
                enemy->hp       -= damage;
                work->field_70A += damage;
                switch ((u16)kind) {
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        break;
                    case 1:
                    case 2:
                        if (work->field_6EC == 0) {
                            work->field_6EC = 1;
                            work->field_6DA = 7;
                        }
                        break;
                    case 9:
                        work->field_70A += GOLEM_KNIGHT_BISHOP_HIT_WEIGHT;
                        break;
                }
                if (lastId != work->field_49C[i].key.value) {
                    lastId     = work->field_49C[i].key.value;
                    sc->ofs.vx = 0;
                    sc->ofs.vy = 0;
                    t          = -0x96;
                    if (work->field_6D2 == 1) {
                        t = 0xC8;
                    }
                    sc->ofs.vz = t;
                    func_800FDB18((u16)Gp_GetIdParam1(work->field_49C[i].key.value),
                                  &arg0->extra.tmd->coords[3], &sc->ofs,
                                  &work->field_65C);
                }
                wait = Gp_GetIdParam2(work->field_49C[i].key.value);
                if (wait > 0) {
                    work->field_6C6 = wait;
                }
                if (work->field_6DA >= 8) {
                    work->field_6EA = 2;
                }
                if (work->field_718 != 1) {
                    golemKnightBishopPickHitReaction(arg0, damage);
                } else {
                    work->field_6F4 = 2;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_584.flags & 1) {
        work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(&work->field_584);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopHitScratch);
}
