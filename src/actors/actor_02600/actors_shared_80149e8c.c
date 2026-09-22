#include "common.h"
#include "actors/actor_202600.h"
#include "actors/actors_shared_80149e8c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/wipsys.h"

void ActorsShared80149e8c(Actor202600* arg0)
{
    Actor202600Work*       work;
    Actor202600HitScratch* head;
    Actor202600HitScratch* s;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         src;
    s32                    movement;
    s32                    lastId;
    u32                    damage;
    s16                    amount;
    s32                    best;
    s32                    push;
    s32                    val;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    z;
    s32                    wallDx;
    s32                    wallDy;
    s32                    wallDz;
    VECTOR*                unit;
    s32                    i;
    s16                    timer;
    s32                    one;
    u32                    kind;

    best   = 0;
    lastId = 0;
    work   = arg0->field_1C;
    coord  = arg0->field_2C->field_8;
    head   = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC);
    s = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) = head - 1;
    enemy                                                 = (GpEnemy*)arg0->field_20;
    work->field_3CC                                       = 0;
    movement                                              = func_800E0C10(work->field_234, &s->delta, 4, NULL);
    if (movement != 0) {
        if (work->field_39A == 2) {
            work->field_3CC = 1;
        }
        switch (movement) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += head[-1].delta.vx.h.hi;
                coord->coord.t[1] += s->delta.vy.h.hi;
                z                  = coord->coord.t[2] + s->delta.vz.h.hi;
                coord->coord.t[2]  = z;
                break;
            case 2:
                coord->coord.t[0] = work->field_35C.vx;
                coord->coord.t[1] = work->field_35C.vy;
                coord->coord.t[2] = work->field_35C.vz;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_234);
    if (work->field_390 != 0) {
        timer           = (u16)work->field_390 - 1;
        work->field_390 = timer;
        if (timer <= 0) {
            work->field_390 = 0;
        }
    }
    one = 1;
    TOUCH_REG_USE(movement, s);
    work->field_3D0 = 0;
    work->field_3BA = 0;
    unit            = &s->unit;
    for (i = 0; i < 2; i++) {
        kind = (u32)work->field_2B4[i].key >> 16;
        if (kind == one)
            goto physical;
        if (kind == 0)
            goto next_contact;
        if (kind == 2)
            goto damage_contact;
        if (kind == 3)
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->field_390 == 0) {
            movement = 0;
            if ((((u32)work->field_2B4[i].key >> 8) & 0x3F) == 0x24) {
                if ((work->field_2B4[i].key & 0x3F) == 0x24) {
                    movement = 1;
                }
            }
            if ((movement != one) || (work->field_3B2 == 0)) {
                src           = Gp_ActorSlots[((u32)work->field_2B4[i].key >> 7) & 1]->extra->coords;
                dx            = src->coord.t[0] - coord->coord.t[0];
                s->delta.vx.w = dx;
                dy            = src->coord.t[1] - coord->coord.t[1];
                s->delta.vy.w = dy;
                dz            = src->coord.t[2] - coord->coord.t[2];
                s->delta.vz.w = dz;
                damage        = Gp_ComputeDamage((u32)work->field_2B4[i].key, SquareRoot0(dx * dx + dy * dy + dz * dz), 0, 0);
                amount        = damage;
                if (movement == 0) {
                    if (work->field_3CA != 0) {
                        amount = (damage << 16) >> 15;
                        Gp_SpawnEff(0x6009C, arg0->field_2C->field_8 + 1, 3, NULL);
                    }
                    if (Gp_RollEnemyChance(enemy, (u32)work->field_2B4[i].key, 0) != 0) {
                        amount = (amount << 16) >> 14;
                        if (work->field_3CA == 0) {
                            Gp_SpawnEff(0x6009C, arg0->field_2C->field_8 + 1, 0, NULL);
                        }
                    }
                    func_800E2C78((GpObj40*)enemy, (u32)work->field_2B4[i].key, amount, 0);
                }
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if (work->field_3C8 != one) {
                    if (enemy->hp <= 0) {
                        work->field_39A = 9;
                        work->field_39C = 0;
                        arg0->field_30  = 2;
                    } else if (movement == 0) {
                        work->field_39A = 6;
                        work->field_39C = 0;
                    }
                }
                if (work->field_3C8 == 2) {
                    if ((work->field_39A == 9) || (movement == 0)) {
                        do {
                            work->field_3C8 = 0;
                            work->field_3A2 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            s->rot.vx       = 0;
                            s->rot.vy       = (u16)work->field_3A2 + 0x800;
                            s->rot.vz       = 0;
                            RotMatrix(&s->rot, &coord->coord);
                        } while (0);
                    }
                }
                if (movement == 0) {
                    work->field_3D0        = one;
                    work->field_2E4.flags &= 0x3FFF;
                }
                switch (Gp_GetIdParam0(work->field_2B4[i].key) & 0xFFFF) {
                    case 0:
                    case 1:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case 2:
                        Gp_SetObjFlag2((GpObj5D*)enemy, work->field_2B4[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4((GpObj5C*)enemy, work->field_2B4[i].key, 0);
                        break;
                    case 4:
                    case 6:
                        if (work->field_3C8 != one) {
                            work->field_3BA = one;
                        }
                        break;
                    case 7:
                        if (work->field_3B0 == 0) {
                            work->field_3B0        = one;
                            work->field_3BE        = 0;
                            work->field_3B2        = 0;
                            work->field_31C.flags |= 0x8000;
                            Gp_SetLightMode((GpObj4C*)arg0->field_20, 3);
                        }
                        break;
                }
                if (lastId != work->field_2B4[i].key) {
                    lastId    = work->field_2B4[i].key;
                    s->rot.vx = 0;
                    s->rot.vy = -0xC8;
                    s->rot.vz = 0;
                    func_800FDB18(Gp_GetIdParam1(work->field_2B4[i].key) & 0xFFFF, arg0->field_2C->field_8 + 1, &s->rot, &work->field_354);
                }
                movement = Gp_GetIdParam2(work->field_2B4[i].key);
                if (movement > 0) {
                    work->field_390 = movement;
                }
            }
        }
        goto next_contact;
    physical:
        wallDx        = coord->workm.t[0] - work->field_2B4[i].point.vx;
        s->delta.vx.w = wallDx;
        wallDy        = coord->workm.t[1] - work->field_2B4[i].point.vy;
        s->delta.vy.w = wallDy;
        wallDz        = coord->workm.t[2] - work->field_2B4[i].point.vz;
        s->delta.vz.w = wallDz;
        push          = work->field_2B4[i].depth - SquareRoot0(wallDx * wallDx + wallDy * wallDy + wallDz * wallDz);
        val           = push;
        if (push <= 0) {
            val = 0;
        }
        push = val;
        if (best < push) {
            best = push;
            VectorNormal((VECTOR*)&s->delta, unit);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, unit, &s->local);
        }
    next_contact:;
    }
    if (best > 0) {
        coord->coord.t[0] += (best * s->local.vx) >> 12;
        coord->coord.t[2] += (best * s->local.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_2B4);
    work->field_3CE = 0;
    if (work->field_304[0].flags & 1) {
        if (((work->field_304[0].key & 0xFFFF0000) == 0x10000) ||
            (((work->field_304[0].key & 0xFFFF0000) == 0x100000) && (work->field_304[0].at10.normal.vy == 0))) {
            do {
                work->field_3CE = 1;
            } while (0);
        }
        work->field_2E4.flags &= 0x3FFF;
        Gp_ClearRec18Occupied(work->field_304);
    }
    *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) = *(Actor202600HitScratch**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}
