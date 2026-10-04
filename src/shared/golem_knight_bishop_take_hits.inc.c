/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Per-frame hit handler: applies the `func_800E0C10` push-back from
/// `groundContacts` and (while `hurtBody` is grid-enabled) `hurtContacts` to
/// the root coordinate, counts `hitCooldown` down and re-arms `hurtBody` when
/// it ends, and for each weapon hit in `hurtContacts` outside the cooldown
/// computes the damage from the distance to the player, applies it to the
/// `Enemy` and to `interruptDamage`, spawns the hit effect once per distinct
/// key and hands the damage to `golemKnightBishopPickHitReaction` - or, while
/// the golem holds the player, raises `grabBreak` instead. A touch or a hit
/// on a feint only sets `feintBroken`. A blow `strikeBody` has landed
/// switches that body off.
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

    switch (func_800E0C10(work->groundContacts, &sc->delta, ARRAY_SIZE(work->groundContacts), NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += sc->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += sc->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    Gp_ClearRec18Occupied(work->groundContacts);

    if (work->hurtBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->hurtContacts, &sc->delta, ARRAY_SIZE(work->hurtContacts), NULL)) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += sc->delta.fixed.vx.halves.integer;
                coord->coord.t[2] += sc->delta.fixed.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->prevRootPos.vx;
                coord->coord.t[2] = work->prevRootPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        t                 = work->hitCooldown - 1;
        work->hitCooldown = t;
        if (t <= 0) {
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->hitCooldown     = 0;
            work->hurtBody.key    = work->actorId | 0x30000;
        }
    }

    for (i = 0; i < ARRAY_SIZE(work->hurtContacts); i++) {
        switch ((u32)work->hurtContacts[i].key.value >> 16) {
            case 0:
                break;
            case 1:
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                }
                break;
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                    break;
                }
                sc->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->hitFromFront  = (u32) ~(sc->delta.vector.vx * coord->coord.m[0][2] +
                                             sc->delta.vector.vy * coord->coord.m[1][2] +
                                             sc->delta.vector.vz * coord->coord.m[2][2]) >>
                                     31;
                damage = Gp_ComputeDamage(work->hurtContacts[i].key.value,
                                          SquareRoot0(sc->delta.vector.vx * sc->delta.vector.vx +
                                                      sc->delta.vector.vy * sc->delta.vector.vy +
                                                      sc->delta.vector.vz * sc->delta.vector.vz),
                                          0, 0);
                kind   = Gp_GetIdParam0(work->hurtContacts[i].key.value);
                if ((u16)kind == 5) {
                    damage *= 2;
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if (Gp_RollEnemyChance(enemy, work->hurtContacts[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((u16)kind != 5) {
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->hurtContacts[i].key.value, damage, 0);
                enemy->hp             -= damage;
                work->interruptDamage += damage;
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
                        if (work->flickerStage == 0) {
                            work->flickerStage = 1;
                            work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                        }
                        break;
                    case 9:
                        work->interruptDamage += GOLEM_KNIGHT_BISHOP_HIT_WEIGHT;
                        break;
                }
                if (lastId != work->hurtContacts[i].key.value) {
                    lastId     = work->hurtContacts[i].key.value;
                    sc->ofs.vx = 0;
                    sc->ofs.vy = 0;
                    t          = -0x96;
                    if (work->hitFromFront == 1) {
                        t = 0xC8;
                    }
                    sc->ofs.vz = t;
                    func_800FDB18((u16)Gp_GetIdParam1(work->hurtContacts[i].key.value),
                                  &arg0->extra.tmd->coords[3], &sc->ofs,
                                  &work->hitEffectArg);
                }
                wait = Gp_GetIdParam2(work->hurtContacts[i].key.value);
                if (wait > 0) {
                    work->hitCooldown = wait;
                }
                if (work->fadeState >= GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM) {
                    work->tintRequest = 2;
                }
                if (work->grabStage != 1) {
                    golemKnightBishopPickHitReaction(arg0, damage);
                } else {
                    work->grabBreak = 2;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->hurtContacts);
    if (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->strikeContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopHitScratch);
}
