#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/tmd.h"

/// Spawn handler: allocates the work area, binds the model and collision
/// objects, and seeds the pose from the spawn variant in `GpAreaPlace.field_1`.
void func_actor_101500_80131EB4(GpEnemy* arg0, Actor101500* arg1)
{
    Actor101500Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    GpRec18*         records;
    u32              draw;
    s32              i;
    s32              r;

    obj   = arg1->field_2C;
    coord = obj->coords;
    work  = memCalloc(0x384U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, (Task*)arg1);
        return;
    }
    arg1->field_1C = work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &arg1->field_2C->coords[2];
    arg0->node.flags           = 0;
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &D_actor_101500_8013BDD8;
    arg0->recs                 = work->field_1FC;
    arg0->hp                   = D_actor_101500_8013BDD8.hpMax;
    work->field_314.coord      = coord;
    work->field_314.spawnArgLo = 0x300;
    work->field_314.spawnArgHi = 1;
    place                      = arg0->place;
    switch (work->field_382 = place->field_1) {
        case 0:
            work->field_36E = arg0->place->field_2 & 1;
            work->field_370 = (arg0->place->field_2 >> 1) & 1;
            switch (work->field_36E) {
                case 0:
                    work->field_352 = 1;
                    work->field_354 = 1;
                    work->field_34C = 0;
                    break;
                case 1:
                    work->field_352 = 2;
                    work->field_354 = 2;
                    work->field_34C = 0;
                    break;
            }
            work->field_362 = 0;
            draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->field_364    = ((draw >> 16) & 0x1F) + 1;
            work->field_372    = (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) + 0x800) & 0xFFF;
            break;
        case 1:
            work->field_370 = 1;
            work->field_352 = 5;
            work->field_354 = 5;
            work->field_35A = 9;
            work->field_35C = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_362 = ((Gp_LcgState >> 16) & 0x3F) + 0x3C;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_364 = (Gp_LcgState >> 16) & 0x1FF;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            break;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    func_800B3F84((GpAnimCtx*)work, D_actor_101500_8013BE34, obj, work->field_12C,
                  (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, (s16)work->field_352);
    }
    if (work->field_382 == 0) {
        draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        r                  = (draw >> 16) & 0x3F;
        for (i = 1; i < 7; i++) {
            func_800B4114(work, i, work->field_352, 0, r);
        }
    }
    work->field_1DC.coord    = &arg1->field_2C->coords[2];
    work->field_1DC.ctx.recs = work->field_1FC;
    work->field_1DC.pos.vx   = 0;
    work->field_1DC.pos.vy   = 0;
    work->field_1DC.pos.vz   = 0;
    work->field_1DC.key      = 0x3000F;
    work->field_1DC.radius   = 0x12C;
    work->field_1DC.flags    = 1;
    Gp_LinkObj(2, &work->field_1DC);
    Gp_InitRec18Table(work->field_1FC, 3, 0);
    work->field_244.ctx.recs = work->field_264;
    work->field_244.coord    = coord;
    work->field_244.pos.vx   = 0;
    work->field_1DC.flags   |= 0x8000;
    if (work->field_36E == 0) {
        work->field_244.pos.vy = 0;
        work->field_244.pos.vz = -0x12C;
    } else {
        work->field_244.pos.vy = 0x12C;
        work->field_244.pos.vz = 0;
    }
    work->field_244.key    = 0x3000F;
    work->field_244.radius = 0x12C;
    work->field_244.flags  = 1;
    Gp_LinkObj(2, &work->field_244);
    Gp_InitRec18Table(work->field_264, 5, 0);
    records                  = work->field_2FC;
    work->field_2DC.coord    = coord;
    work->field_2DC.ctx.recs = records;
    work->field_2DC.pos.vx   = 0;
    work->field_2DC.pos.vy   = 0;
    work->field_2DC.pos.vz   = 0x190;
    work->field_244.flags   |= 0x4200;
    work->field_2DC.key      = Gp_PackPair(&D_actor_101500_8013BDD4, 0);
    work->field_2DC.radius   = 0x12C;
    work->field_2DC.flags    = 1;
    Gp_LinkObj(3, &work->field_2DC);
    Gp_InitRec18Table(records, 1, 0);
    work->field_2DC.flags &= 0x7FFF;
    arg1->field_30         = 1;
}

/// Per-frame contact pass: applies the collision step, reacts to the three
/// contact records (damage from actors, push-out from walls) and clears them.
void func_actor_101500_8013230C(Actor101500* actor)
{
    Actor101500Work*         work;
    Actor101500ContactFrame* frame;
    s32                      push;
    VECTOR*                  normal;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           sourceCoord;
    GpRec18*                 effectRec;
    s16                      cooldown;
    s32                      result;
    s32                      i;
    s32                      depth;
    s32                      boundedDepth;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    s32                      wallDx;
    s32                      wallDy;
    s32                      wallDz;
    u32                      lastId;
    u32                      id;
    u32                      hitId;
    u32                      damage;

    push                                    = 0;
    lastId                                  = 0;
    work                                    = actor->field_1C;
    *(Actor101500ContactFrame**)0x1F8003FC -= 1;
    frame                                   = *(Actor101500ContactFrame**)0x1F8003FC;
    coord                                   = actor->field_2C->coords;
    result                                  = func_800E0C10(work->field_264, &frame->delta, 5, NULL);
    if (result != 0) {
        if (work->field_370 == 0 && work->field_35A == 3 && frame->delta.vy.w == 0) {
            work->field_35A        = 6;
            work->field_358        = 0;
            work->field_244.pos.vy = 0;
            work->field_244.pos.vz = -300;
            Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
            work->field_362        = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            for (i = 0; i < 5; i++) {
                if ((work->field_264[i].key & 0xFFFF0000) == 0x100000) {
                    frame->dx       = work->field_264[i].at10.normal.vx;
                    frame->dz       = work->field_264[i].at10.normal.vz;
                    work->field_372 = (ratan2(frame->dx, frame->dz) + 0x800) & 0xFFF;
                    break;
                }
            }
        }
        if (work->field_35A == 4 && frame->delta.vy.w < -0xDDA) {
            work->field_35A = 5;
            work->field_362 = 0;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.vx.h.hi;
                coord->coord.t[1] += frame->delta.vy.h.hi;
                coord->coord.t[2] += frame->delta.vz.h.hi;
                break;
            case 2:
                if (work->field_35A != 4) {
                    coord->coord.t[0] = work->field_31C.vx;
                    coord->coord.t[1] = work->field_31C.vy;
                    coord->coord.t[2] = work->field_31C.vz;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_264);
    if (work->field_350 != 0) {
        cooldown        = (u16)work->field_350 - 1;
        work->field_350 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_350 = 0;
        }
    }
    normal = &frame->normal;
    for (i = 0; i < 3; i++) {
        id = work->field_1FC[i].key;
        switch (id >> 0x10) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_350 == 0) {
                    sourceCoord       = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
                    dx                = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vx.w = dx;
                    dy                = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vy.w = dy;
                    dz                = sourceCoord->coord.t[2] - coord->coord.t[2];
                    frame->delta.vz.w = dz;
                    damage            = Gp_ComputeDamage(work->field_1FC[i].key, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (Gp_RollEnemyChance(actor->field_20, work->field_1FC[i].key, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(0x6009C, actor->field_2C->coords, 0, NULL);
                    }
                    func_800DA6E8(&actor->field_20->node, damage, 0);
                    func_800E2C78((GpObj40*)actor->field_20, work->field_1FC[i].key, damage, 0);
                    func_actor_101500_8013291C(actor, damage);
                    switch (Gp_GetIdParam0(work->field_1FC[i].key) & 0xFFFF) {
                        case 0:
                        case 5:
                        case 7:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)actor->field_20);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)actor->field_20, work->field_1FC[i].key, 0);
                            break;
                        case 4:
                        case 6:
                            if (actor->field_20->hp <= 0) {
                                work->field_37E = 1;
                            }
                            break;
                        case 2:
                        case 8:
                        case 9:
                            Gp_SetObjFlag2((GpObj5D*)actor->field_20, work->field_1FC[i].key, 0);
                            break;
                    }
                    hitId = work->field_1FC[i].key;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(hitId) & 0xFFFF, coord, NULL, &work->field_314);
                    }
                    damage = Gp_GetIdParam2(work->field_1FC[i].key);
                    if ((s32)damage > 0) {
                        work->field_350 = damage;
                    }
                }
                break;
            case 3:
                wallDx            = coord->workm.t[0] - work->field_1FC[i].point.vx;
                frame->delta.vx.w = wallDx;
                wallDy            = coord->workm.t[1] - work->field_1FC[i].point.vy;
                frame->delta.vy.w = wallDy;
                wallDz            = coord->workm.t[2] - work->field_1FC[i].point.vz;
                frame->delta.vz.w = wallDz;
                depth             = work->field_1FC[i].depth - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
                boundedDepth      = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, &frame->push);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * frame->push.vx) >> 0xC;
        coord->coord.t[2] += (s32)(push * frame->push.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1FC);
    effectRec = work->field_2FC;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_2DC.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
        work->field_36A = 1;
    }
    *(Actor101500ContactFrame**)0x1F8003FC += 1;
}

void func_actor_101500_8013291C(Actor101500* actor, s32 damage)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              id;

    enemy      = actor->field_20;
    work       = actor->field_1C;
    coord      = actor->field_2C->coords;
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        work->field_378 = 1;
    }
    id = ((actor->field_20->placeKey >> 12) << 8) | 0x400F0004;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    if (enemy->hp <= (D_actor_101500_8013BDDC * 60) / 100) {
        work->field_358 = 2;
        if (work->field_35A != 5) {
            work->field_35A        = 4;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
        }
    } else {
        work->field_35A = 7;
        switch (work->field_358) {
            case 0:
                if (work->field_36E == 0) {
                    work->field_352 = 11;
                    work->field_354 = 1;
                } else {
                    work->field_352 = 12;
                    work->field_354 = 2;
                }
                break;
            case 1:
                work->field_352 = 13;
                work->field_354 = 6;
                break;
            case 2:
                work->field_352 = 13;
                work->field_354 = 14;
                break;
        }
        work->field_34C = 0;
        work->field_356 = 0;
    }
    Gp_SetStateF0Byte3(2);
}

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_80132AC4);
