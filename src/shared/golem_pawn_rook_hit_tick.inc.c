#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Hit and push tick. Applies the `groundContacts` / `hurtContacts` collision deltas
/// to the root coordinate, then walks the `hurtContacts` records: kind 2 is a
/// weapon hit (damage, crit roll, the `shieldHp` shield budget, and the
/// reaction animation picked into `behavior`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `playerSpotted` when the player's segment test against `sightContacts` fails.
void golemPawnRookTakeHits(Task* arg0)
{
    s32                      result;
    s32                      maxPush;
    s32                      hit;
    u32                      lastId;
    GolemPawnRookWork*       work;
    GolemPawnRookHitScratch* head;
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
    work    = arg0->work;
    head    = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    self    = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(GolemPawnRookHitScratch);
    scratch = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    enemy   = (Enemy*)arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->groundContacts, &head[-1].delta, ARRAY_SIZE(work->groundContacts), NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            self->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            self->coord.t[0] = work->prevRootPos.vx;
            self->coord.t[1] = work->prevRootPos.vy;
            self->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);

    if (work->hurtBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->hurtContacts, &scratch->delta, ARRAY_SIZE(work->hurtContacts), NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case 2:
                self->coord.t[0] = work->prevRootPos.vx;
                self->coord.t[2] = work->prevRootPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(work->hurtContacts); i++) {
        switch ((u32)work->hurtContacts[i].key.value >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                other                    = gPlayerActorTasks[((u32)work->hurtContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->delta.vector.vx = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vector.vy = other->coord.t[1] - self->coord.t[1];
                dz                       = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vector.vz = dz;
                val                      = (scratch->delta.vector.vx * self->coord.m[0][2]) + (scratch->delta.vector.vy * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->hitFromFront       = val >= 0;
                damage                   = Gp_ComputeDamage(work->hurtContacts[i].key.value,
                                                            SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)),
                                                            0, 0);
                kind                     = Gp_GetIdParam0(work->hurtContacts[i].key.value);
                if (work->shieldRaised != 0 && work->hitFromFront == 1 && work->downedPose == 0) {
                    if (work->hurtContacts[i].key.value & 0x8000) {
                        if (gGolemPawnRookWeakPointPe[work->hurtContacts[i].key.value & 0x7F] != 0) {
                            hit             = 1;
                            work->shieldHp -= damage;
                        }
                    } else if (gGolemPawnRookWeakPointWeapons[work->hurtContacts[i].key.value & 0x7F] != 0) {
                        hit             = 1;
                        work->shieldHp -= damage;
                    }
                    if (hit == 1) {
                        if (work->shieldHp <= 0) {
                            work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                            work->shieldRaised      = 0;
                            work->shieldBreakStep   = 1;
                            work->step              = 0;
                            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            if (work->screamEffect != NULL) {
                                work->screamEffect->task->state = 3;
                                work->screamEffect              = NULL;
                            }
                        }
                        worldTargetAddReadoutAmount(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->hurtContacts[i].key.value);
                        if (cooldown > 0) {
                            work->hitCooldown = cooldown;
                        }
                        break;
                    }
                } else {
                    work->shieldRaised = 0;
                    if ((kind & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                }
                if (damageRollCriticalHit(enemy, work->hurtContacts[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    if (work->buildupActive == 0) {
                        result = 1;
                    }
                }
                if (work->screamCharges != 0 && (work->hurtContacts[i].key.value & 0x8000)) {
                    damage >>= 2;
                }
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                damageAccumulateLifeDrainHp(enemy, work->hurtContacts[i].key.value, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->downedPose == 0) {
                        result = 5;
                    } else {
                        result = 6;
                    }
                } else if (enemy->hp < enemy->param->hpMax * 15 / 100) {
                    if (work->downedPose == 0) {
                        result = 3;
                    } else {
                        result = 4;
                    }
                }
                if (work->attackActive != 0 || work->screamActive != 0) {
                    work->interruptDamage += damage;
                }
                switch (kind & 0xFFFF) {
                    case 1:
                        if (work->screamCharges == 0 && work->downedPose == 0 && result < 3 && work->buildupActive == 0) {
                            result = 2;
                        }
                        break;
                    case 2:
                        if (work->screamCharges == 0 && work->downedPose == 0 && result < 3) {
                            Gp_SetObjFlag2(enemy, work->hurtContacts[i].key.value, 0);
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
                if (lastId != work->hurtContacts[i].key.value) {
                    lastId                   = work->hurtContacts[i].key.value;
                    scratch->effectOffset.vx = 0;
                    scratch->effectOffset.vy = 0;
                    scratch->effectOffset.vz = (work->hitFromFront == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->hurtContacts[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3],
                                  &scratch->effectOffset, &work->hitEffectArg);
                }
                cooldown = Gp_GetIdParam2(work->hurtContacts[i].key.value);
                if (cooldown > 0) {
                    work->hitCooldown = cooldown;
                }
                switch (result) {
                    case 0:
                        if (work->behavior < GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE) {
                            work->anim     = 2;
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                            work->step     = 0;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        rng             = gRandomLcgState >> 16;
                        tilt            = (rng & 0x7F) + 0x40;
                        if (!(rng & 1)) {
                            tilt = -tilt;
                        }
                        work->hitTilt.vx = tilt;
                        byte1            = (s16)rng >> 8;
                        val              = (byte1 & 0x7F) + 0x40;
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->hitTilt.vy    = val;
                        work->hitTiltActive = 1;
                        break;
                    case 1:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                        work->step              = 0;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 2:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                        work->step              = 0;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 3:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                        work->step              = 0;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 4:
                        if (work->fallingDown == 0) {
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_HIT;
                            work->step     = 0;
                        }
                        break;
                    case 5:
                        work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_COLLAPSE;
                        work->step              = 0;
                        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 6:
                        if (work->fallingDown == 0) {
                            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH;
                            work->step     = 0;
                        }
                        break;
                }
                if (result != 0 && work->screamEffect != NULL) {
                    work->screamEffect->task->state = 3;
                    work->screamEffect              = NULL;
                }
                break;
            case 3:
                part                     = &arg0->extra.tmd->coords[3];
                x                        = part->workm.t[0] - work->hurtContacts[i].point.vx;
                scratch->delta.vector.vx = x;
                y                        = part->workm.t[1] - work->hurtContacts[i].point.vy;
                scratch->delta.vector.vy = y;
                z                        = part->workm.t[2] - work->hurtContacts[i].point.vz;
                scratch->delta.vector.vz = z;
                push                     = work->hurtContacts[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped                  = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->pushDirection.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->pushDirection.vz) >> 12;
    }
    worldCollisionClearContacts(work->hurtContacts);
    if (work->strikeContacts[0].flags & 1) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->strikeContacts);
    }
    work->playerSpotted = 0;
    if (worldCollisionCountContactsByKind(work->sightContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        part                     = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4];
        scratch->effectOffset.vx = part->workm.t[0];
        scratch->effectOffset.vy = part->workm.t[1];
        scratch->effectOffset.vz = part->workm.t[2];
        scratch->rootPos.vx      = self->workm.t[0];
        scratch->rootPos.vy      = self->workm.t[1];
        scratch->rootPos.vz      = self->workm.t[2];
        if (detectSegmentHitsWall(&scratch->effectOffset, &scratch->rootPos) == 0) {
            work->playerSpotted = 1;
        }
    }
    worldCollisionClearContacts(work->sightContacts);
    SCRATCH_STACK_RELEASE_BLOCK(GolemPawnRookHitScratch);
}
