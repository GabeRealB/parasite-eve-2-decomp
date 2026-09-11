#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "actors/actor_342400.h"
#include "psyq/abs.h"

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 pick_step(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`field_2`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void calc_push(Task* arg0, GsCOORDINATE2* coord, GpRec18* rec, SVECTOR* out)
{
    SVECTOR        pos;
    VECTOR         d;
    VECTOR         n;
    GsCOORDINATE2* c2;
    s32            t;
    s32            pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = ((TmdObject*)arg0->extra)->field_8;
    d.vx   = pos.vx - rec->field_8;
    d.vy   = 0;
    d.vz   = pos.vz - rec->field_C;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->field_2 - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->field_8;
    d.vy = c2->workm.t[1] - rec->field_A;
    d.vz = c2->workm.t[2] - rec->field_C;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Per-frame contact handling for the overlay's enemy. Walks the eight
/// contact records: kind 1 (skipped when `arg1` is set) and kind 3 push the
/// model out, kind 2 applies a hit - damage, status effects and the pending
/// state request in `field_448` - unless `field_40E` is still cooling down.
/// Then ticks the status flags, applies `func_800E0C10`'s collision step
/// (snapping back to `field_60` when it reports a conflict) and moves the
/// root by the combined step and push-out.
void func_actor_342400_801653DC(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor342400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ                  = 0;
    maxX                   = 0;
    maxZ                   = 0;
    stepX                  = 0;
    blocked                = 0;
    work                   = (Actor342400Work*)arg0->idMap;
    coord                  = ((TmdObject*)arg0->extra)->field_8;
    enemy                  = arg0->spawnArg2;
    *(u8**)G_SCRATCH_HEAD -= 8;
    work->field_41E        = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].field_4 & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                calc_push(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].field_4, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].field_4);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].field_4, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->field_8[3], 0, NULL);
                    }
                    func_800E2C78((GpObj40*)enemy, work->rec_2EC[i].field_4, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->field_40 -= amount;
                    if (enemy->field_40 < 0) {
                        enemy->field_40 = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF,
                                  &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].field_4) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->field_4C & 1) {
        enemy->field_4C &= 0xFE;
        work->field_448  = 5;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_448  = 3;
    }
    if (enemy->field_4C & 0xC) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->field_40 -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->field_40 < 0) {
                enemy->field_40 = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_60.vx;
            coord->coord.t[2] = work->field_60.vz;
            coord->flg        = 0;
            blocked           = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80    += pick_step(stepX, maxX >> 3);
        work->field_84    += pick_step(stepZ, maxZ >> 3);
        coord->coord.t[0] += pick_step(stepX, maxX >> 3);
        coord->coord.t[2] += pick_step(stepZ, maxZ >> 3);
        coord->flg         = 0;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}
