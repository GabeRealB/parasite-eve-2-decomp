#include "common.h"
#include "actors/actor_100700.h"
#include "actors/actors_shared_80135b58.h"
#include "main/session.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
void func_800DA6E8(void*, s32, s32);

void Actor00700_Fn00334(Actor00700* actor)
{
    Actor00700Ctx*  ctx;
    u32             lastId;
    Actor00700Work* work;
    Actor00700Work* contactWork;
    GpDeltaScratch* allocated;
    GpDeltaScratch* oldScratch;
    GpDeltaScratch* scratch;
    GpDeltaScratch* normal;
    GsCOORDINATE2*  coord;
    s16             cooldown;
    s16             health;
    s32             cooldownParam;
    s32             wallDx;
    s32             wallDy;
    s32             wallDz;
    s32             result;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             depth;
    s32             push;
    s32             z;
    s32             boundedDepth;
    s8*             effectRec;
    s8*             contactRec;
    u32             id;
    u32             kind;
    u32             hitId;
    u32             effect;
    u32             damage;
    GsCOORDINATE2*  sourceCoord;

    push       = 0;
    lastId     = 0;
    oldScratch = *(GpDeltaScratch**)0x1F8003FC;
    work       = actor->field_1C;
    allocated  = oldScratch - 3;
    SOFT_TOUCH_REG(allocated);
    scratch                       = allocated;
    *(GpDeltaScratch**)0x1F8003FC = scratch;
    coord                         = actor->field_2C->field_8;
    ctx                           = actor->field_20;
    result                        = func_800E0C10((GpRec18*)&work->field_27C[0x20], scratch, 4, NULL);
    USE_REG(oldScratch);
    if (result == 1)
        goto move_delta;
    if (result < 2)
        goto move_done;
    if (result == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += oldScratch[-3].vx.h.hi;
    coord->coord.t[1] += scratch->vy.h.hi;
    z                  = coord->coord.t[2] + scratch->vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_360;
    coord->coord.t[1] = work->field_364;
    z                 = work->field_368;
move_z:
    coord->coord.t[2] = z;
move_done:
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        cooldown        = (u16)work->field_378 - 1;
        work->field_378 = cooldown;
        normal          = scratch + 1;
        if ((cooldown << 0x10) <= 0) {
            work->field_378 = 0;
            goto cooldown_done;
        }
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG2(scratch, scratch);
    contactWork = work;
contact_loop: {
    USE_REG2(contactWork, contactWork);
    id   = contactWork->field_22C.contacts.recs[0].field_4;
    kind = id >> 0x10;
    if (kind == 1)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor00700Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_378 == 0) {
        sourceCoord   = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = Gp_ComputeDamage(contactWork->field_22C.contacts.recs[0].field_4, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
        USE_REG(damage);
        if (Gp_RollEnemyChance((GpEnemy*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, actor->field_2C->field_8, 0, NULL);
        }
        func_800DA6E8(&actor->field_20->node, (s32)damage, 0);
        func_800E2C78((GpObj40*)actor->field_20, (s32)contactWork->field_22C.contacts.recs[0].field_4, (s32)damage, 0);
        health        = (u16)ctx->field_40 - damage;
        ctx->field_40 = health;
        if ((health << 0x10) <= 0) {
            work->field_37A = 5;
            work->field_37C = 0;
            actor->field_30 = (s32)kind;
        } else if (work->field_398 == 0) {
            work->field_37A = 4;
            work->field_37C = 0;
        }
        work->field_31A &= 0x7FFF;
        effect           = Gp_GetIdParam0(contactWork->field_22C.contacts.recs[0].field_4) & 0xFFFF;
        switch (effect) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0);
                break;
            case 1:
            case 9:
                Gp_SetObjFlag1((GpObj4C*)actor->field_20);
                break;
        }
        hitId = contactWork->field_22C.contacts.recs[0].field_4;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1((s32)hitId) & 0xFFFF, coord, NULL, (GpEffArg*)&work->pad_31C[0x18]);
        }
        cooldownParam = Gp_GetIdParam2(contactWork->field_22C.contacts.recs[0].field_4);
        if (cooldownParam > 0) {
            work->field_378 = cooldownParam;
        }
    }
    goto next_contact;
physical_contact:
    wallDx        = coord->workm.t[0] - contactWork->field_22C.contacts.recs[0].field_8;
    scratch->vx.w = wallDx;
    wallDy        = coord->workm.t[1] - contactWork->field_22C.contacts.recs[0].field_A;
    scratch->vy.w = wallDy;
    wallDz        = coord->workm.t[2] - contactWork->field_22C.contacts.recs[0].field_C;
    scratch->vz.w = wallDz;
    depth         = contactWork->field_22C.contacts.recs[0].field_2 - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
    boundedDepth  = depth;
    if (depth <= 0) {
        boundedDepth = 0;
    }
    SOFT_TOUCH_REG_USE(boundedDepth, depth);
    depth = boundedDepth;
    if (push < depth) {
        push = depth;
        VectorNormal((VECTOR*)scratch, (VECTOR*)normal);
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)normal, (VECTOR*)(scratch + 2));
    }
next_contact:
    contactWork = (Actor00700Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->pad_0[0x48])
        goto contact_loop;
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied((s8*)work->field_22C.contacts.recs);
    effectRec = work->pad_31C;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->field_1FC;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = Gp_ActorSlots[(u8)work->field_1FC[4] >> 7]->extra->coords;
        work->field_394  = 1;
        work->field_1FA &= 0x7FFF;
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    *(GpDeltaScratch**)0x1F8003FC += 3;
}
