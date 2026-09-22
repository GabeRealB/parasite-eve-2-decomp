#include "common.h"

#include "actors/actor_103800.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Gp_ArmStateF0(s32 arg0);

void Actor03800_Fn00A98(Actor103800* arg0)
{
    s32              one;
    u32              lastId;
    Actor103800Ctx*  ctx;
    Actor103800Work* work;
    Actor103800Work* contactWork;
    Actor103800Work* wallWork;
    GpDeltaScratch*  scratch;
    GpDeltaScratch*  normal;
    GsCOORDINATE2*   coord;
    s16              health;
    s16              state;
    s32              id;

    s32            dx2;
    s32            dy2;
    s32            dz2;
    s32            result;
    s32            dx;
    s32            dy;
    s32            dz;
    s32            hitId;
    s32            depth;
    s32            i;
    s32            reaction;
    s32            push;
    s32            boundedDepth;
    u32            kind;
    u32            effect;
    u32            damage;
    GsCOORDINATE2* sourceCoord;

    push     = 0;
    reaction = 0;
    lastId   = 0;
    work     = arg0->field_1C;
    scratch  = (*(GpDeltaScratch**)0x1F8003FC -= 3);
    coord    = work->field_344;
    ctx      = arg0->field_20;
    result   = func_800E0C10(work->field_22C, scratch, 4, NULL);
    i        = 0;
    if (result != 0) {
        for (i = 0; i < 4; i++) {
            wallWork = (Actor103800Work*)((u8*)work + i * 0x18);
            if ((wallWork->field_22C[0].key & 0xFFFF0000) == 0x100000) {
                if (wallWork->field_22C[0].at10.normal.vy >= -0xDDA) {
                    if (work->field_36E == 0) {
                        work->field_370 = 1;
                        break;
                    }
                } else {
                    work->field_374 = 1;
                }
            }
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += scratch->vx.h.hi;
                coord->coord.t[1] += scratch->vy.h.hi;
                coord->coord.t[2] += scratch->vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_2EC;
                coord->coord.t[1] = work->field_2EE;
                coord->coord.t[2] = work->field_2F0;
                break;
            default:
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_22C);
    USE_REG4(scratch, scratch, scratch, scratch);
    if (work->field_34E != 0) {
        if (--work->field_34E <= 0) {
            work->field_34E = 0;
            goto cooldown_done;
        }
        normal = scratch + 1;
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    one         = 1;
    contactWork = work;
contact_loop: {
    id   = contactWork->field_1C4[0].key;
    kind = (u32)id >> 0x10;
    if (kind == one)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor103800Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_34E == 0) {
        sourceCoord   = Gp_ActorSlots[((u32)id >> 7) & 1]->extra->coords;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
        damage        = Gp_ComputeDamage(contactWork->field_1C4[0].key, damage, 0, 0);
        if (work->field_36E == 0) {
            if (Gp_RollEnemyChance((GpEnemy*)ctx, (u32)contactWork->field_1C4[0].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
        } else if (!(contactWork->field_1C4[0].key & 0x8000) && (damage != 0)) {
            damage *= 3;
            Gp_SpawnEff(0x6009C, coord, 4, NULL);
        }
        func_800DA6E8(&ctx->node, (s32)damage, 0);
        func_800E2C78((GpObj40*)ctx, contactWork->field_1C4[0].key, (s32)damage, 0);
        health        = (u16)ctx->field_40 - damage;
        ctx->field_40 = health;
        if ((health << 0x10) <= 0) {
            reaction = 2;
        }
        effect = Gp_GetIdParam0(contactWork->field_1C4[0].key) & 0xFFFF;
        switch (effect) {
            case 0:
                break;
            default:
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)ctx, contactWork->field_1C4[0].key, 0);
                break;
            case 4:
                if (ctx->field_40 > 0) {
                    if (work->field_36E == 0) {
                        reaction = 1;
                    }
                } else {
                    work->field_368 = one;
                }
                break;
            case 6:
                if (ctx->field_40 <= 0) {
                    work->field_368 = one;
                } else if (work->field_36E == 0) {
                    reaction = 1;
                }
                break;
            case 8:
                if ((work->field_36E == 0) && (reaction == 0)) {
                    Gp_SetObjFlag2((GpObj5D*)ctx, contactWork->field_1C4[0].key, 0);
                }
                break;
            case 1:
            case 2:
            case 5:
            case 9:
                if ((work->field_36E == 0) && (reaction == 0)) {
                    reaction = 1;
                }
                break;
        }
        if (reaction == one)
            goto state_one;
        if (reaction < 2) {
            if (reaction == 0)
                goto state_zero;
        } else {
            if (reaction == 2)
                goto state_two;
        }
        goto state_done;
    state_zero:
        if (work->field_352 != 0xA && damage != 0) {
            work->field_352 = 5;
            goto state_reset;
        }
        goto state_done;
    state_one:
        state = 3;
        goto state_assign;
    state_two:
        state = 7;
    state_assign:
        work->field_352 = state;
    state_reset:
        work->field_354 = 0;
    state_done:
        hitId = contactWork->field_1C4[0].key;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, arg0->field_2C->field_8 + 3, NULL, &work->field_2C4);
        }
        result = Gp_GetIdParam2(contactWork->field_1C4[0].key);
        if (result > 0) {
            work->field_34E = (s16)result;
        }
    }
    goto next_contact;
physical_contact:
    dx2           = coord->workm.t[0] - contactWork->field_1C4[0].point.vx;
    scratch->vx.w = dx2;
    dy2           = coord->workm.t[1] - contactWork->field_1C4[0].point.vy;
    scratch->vy.w = dy2;
    dz2           = coord->workm.t[2] - contactWork->field_1C4[0].point.vz;
    scratch->vz.w = dz2;
    depth         = contactWork->field_1C4[0].depth - SquareRoot0((dx2 * dx2) + (dy2 * dy2) + (dz2 * dz2));
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
    contactWork = (Actor103800Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->slots[1].timeLeft)
        goto contact_loop;
    if ((push > 0) && (work->field_350 == 0)) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    work->field_36C = 0;
    result          = Gp_CountRec18Hi(work->field_2AC, 0x10000);
    if (result != 0) {
        Gp_ArmStateF0(1);
        work->field_36C = 1;
        work->field_37A = 1;
        if ((work->field_350 == 0) && (work->field_36E == 0) && (work->field_352 != 0xC)) {
            work->field_352 = 0xC;
            work->field_354 = 0;
        }
    }
    Gp_ClearRec18Occupied(work->field_2AC);
    *(GpDeltaScratch**)0x1F8003FC += 3;
}
