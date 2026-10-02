#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void golemPawnRookTakeHits(Task* arg0)
{
    s32                      result;
    s32                      maxPush;
    s32                      hit;
    u32                      lastId;
    GolemPawnRookWork*       work;
    GpDeltaScratch*          head;
    GolemPawnRookHitScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                self;
    GfxCoord*                other;
    GfxCoord*                part;
    s32                      i;
    s32                      x, y, z;
    s32                      damage;
    s32                      kind;
    s32                      dz;
    s32                      clamped;
    s32                      val;
    s32                      push;
    s16                      cooldown;
    u32                      rng;
    s32                      tilt;
    s32                      byte1;

    result  = 0;
    maxPush = 0;
    hit     = 0;
    lastId  = 0;
    work    = (GolemPawnRookWork*)arg0->work;
    head    = SCRATCH_STACK_CURSOR(GpDeltaScratch);
    self    = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(GolemPawnRookHitScratch);
    scratch = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    enemy   = (Enemy*)arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_584, head - 4, 4, NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-4].vx.halves.integer;
            self->coord.t[1] += scratch->delta.vy.halves.integer;
            self->coord.t[2] += scratch->delta.vz.halves.integer;
            break;
        case 2:
            self->coord.t[0] = work->field_678;
            self->coord.t[1] = work->field_67C;
            self->coord.t[2] = work->field_680;
            break;
    }
    Gp_ClearRec18Occupied(work->field_584);

    if (work->field_4CC.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->field_4EC, &scratch->delta, 5, NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.vx.halves.integer;
                self->coord.t[2] += scratch->delta.vz.halves.integer;
                break;
            case 2:
                self->coord.t[0] = work->field_678;
                self->coord.t[2] = work->field_680;
                break;
        }
    }

    if (work->field_69A != 0) {
        if (--work->field_69A <= 0) {
            work->field_69A = 0;
        }
    }

    for (i = 0; i < 5; i++) {
        switch ((u32)work->field_4EC[i].key.value >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_69A != 0) {
                    break;
                }
                other                  = gPlayerActorTasks[((u32)work->field_4EC[i].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->delta.vx.word = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vy.word = other->coord.t[1] - self->coord.t[1];
                dz                     = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vz.word = dz;
                val                    = (scratch->delta.vx.word * self->coord.m[0][2]) + (scratch->delta.vy.word * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->field_6AA        = val >= 0;
                damage                 = Gp_ComputeDamage(work->field_4EC[i].key.value,
                                                          SquareRoot0((scratch->delta.vx.word * scratch->delta.vx.word) + (scratch->delta.vy.word * scratch->delta.vy.word) + (scratch->delta.vz.word * scratch->delta.vz.word)),
                                                          0, 0);
                kind                   = Gp_GetIdParam0(work->field_4EC[i].key.value);
                if (work->field_6CE != 0 && work->field_6AA == 1 && work->field_6B8 == 0) {
                    if (work->field_4EC[i].key.value & 0x8000) {
                        if (gGolemPawnRookWeakPointPe[work->field_4EC[i].key.value & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (gGolemPawnRookWeakPointWeapons[work->field_4EC[i].key.value & 0x7F] != 0) {
                        hit              = 1;
                        work->field_6D0 -= damage;
                    }
                    if (hit == 1) {
                        if (work->field_6D0 <= 0) {
                            work->field_6A6        = 9;
                            work->field_6CE        = 0;
                            work->field_6D2        = 1;
                            work->field_6A8        = 0;
                            work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            if (work->field_690 != NULL) {
                                work->field_690->task->state = 3;
                                work->field_690              = NULL;
                            }
                        }
                        func_800DA6E8(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
                        if (cooldown > 0) {
                            work->field_69A = cooldown;
                        }
                        break;
                    }
                } else {
                    work->field_6CE = 0;
                    if ((kind & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    if (work->field_6E0 == 0) {
                        result = 1;
                    }
                }
                if (work->field_6C4 != 0 && (work->field_4EC[i].key.value & 0x8000)) {
                    damage >>= 2;
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_4EC[i].key.value, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->field_6B8 == 0) {
                        result = 5;
                    } else {
                        result = 6;
                    }
                } else if (enemy->hp < enemy->param->hpMax * 15 / 100) {
                    if (work->field_6B8 == 0) {
                        result = 3;
                    } else {
                        result = 4;
                    }
                }
                if (work->field_6CC != 0 || work->field_6C2 != 0) {
                    work->field_6B6 += damage;
                }
                switch (kind & 0xFFFF) {
                    case 1:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3 && work->field_6E0 == 0) {
                            result = 2;
                        }
                        break;
                    case 2:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3) {
                            Gp_SetObjFlag2(enemy, work->field_4EC[i].key.value, 0);
                            result = 1;
                        }
                        break;
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                if (lastId != work->field_4EC[i].key.value) {
                    lastId             = work->field_4EC[i].key.value;
                    scratch->effOfs.vx = 0;
                    scratch->effOfs.vy = 0;
                    scratch->effOfs.vz = (work->field_6AA == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3],
                                  &scratch->effOfs, &work->field_670);
                }
                cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
                if (cooldown > 0) {
                    work->field_69A = cooldown;
                }
                switch (result) {
                    case 0:
                        if (work->field_6A6 < 2) {
                            work->field_694 = 2;
                            work->field_6A6 = 2;
                            work->field_6A8 = 0;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        rng             = gRandomLcgState >> 16;
                        tilt            = (rng & 0x7F) + 0x40;
                        if (!(rng & 1)) {
                            tilt = -tilt;
                        }
                        work->field_688.vx = tilt;
                        byte1              = (s16)rng >> 8;
                        val                = (byte1 & 0x7F) + 0x40;
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->field_688.vy = val;
                        work->field_6B4    = 1;
                        break;
                    case 1:
                        work->field_6A6        = 8;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 2:
                        work->field_6A6        = 9;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 3:
                        work->field_6A6        = 0xB;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 4:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xC;
                            work->field_6A8 = 0;
                        }
                        break;
                    case 5:
                        work->field_6A6        = 0xD;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 6:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xE;
                            work->field_6A8 = 0;
                        }
                        break;
                }
                if (result != 0 && work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                    work->field_690              = NULL;
                }
                break;
            case 3:
                part                   = &arg0->extra.tmd->coords[3];
                x                      = part->workm.t[0] - work->field_4EC[i].point.vx;
                scratch->delta.vx.word = x;
                y                      = part->workm.t[1] - work->field_4EC[i].point.vy;
                scratch->delta.vy.word = y;
                z                      = part->workm.t[2] - work->field_4EC[i].point.vz;
                scratch->delta.vz.word = z;
                push                   = work->field_4EC[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped                = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->push.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->push.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_4EC);
    if (work->field_604[0].flags & 1) {
        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->field_604);
    }
    work->field_6B2 = 0;
    if (Gp_CountRec18Hi(work->field_4B4, 0x10000) != 0) {
        part               = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4];
        scratch->effOfs.vx = part->workm.t[0];
        scratch->effOfs.vy = part->workm.t[1];
        scratch->effOfs.vz = part->workm.t[2];
        scratch->target.vx = self->workm.t[0];
        scratch->target.vy = self->workm.t[1];
        scratch->target.vz = self->workm.t[2];
        if (detectSegmentHitsWall(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}
