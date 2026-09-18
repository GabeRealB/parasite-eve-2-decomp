#include "common.h"

#include "actors/actor_101600.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>

extern s32 Gp_LcgState;

void Actor01600_Fn001F4(Actor01600Ctx* ctx, Actor01600* actor)
{
    SVECTOR         sp18;
    GsCOORDINATE2*  next_coord;
    s32             i;
    u32             random;
    GsCOORDINATE2*  coord;
    TmdObject*      obj;
    Actor01600Work* work;
    MATRIX*         matrix;

    obj        = actor->field_2C;
    coord      = obj->field_8;
    work       = Mem_Calloc(0x558U, false);
    next_coord = coord + 1;
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C                       = work;
    obj->field_C                          = 0;
    coord->flg                            = 0;
    obj->field_1C                         = &work->field_22C;
    obj->field_20                         = &work->field_20C;
    work->field_24C.sub                   = &Gfx_ViewCoord;
    matrix                                = &work->field_24C.coord;
    *(s32*)&work->field_24C.coord.m[0][0] = 0x1000;
    *(s32*)&matrix->m[0][2]               = 0;
    *(s32*)&matrix->m[1][1]               = 0x1000;
    *(s32*)&matrix->m[2][0]               = 0;
    matrix->m[2][2]                       = 0x1000;
    work->field_24C.coord.t[0]            = (s32)coord->coord.t[0];
    work->field_24C.coord.t[1]            = (s32)coord->coord.t[1];
    work->field_24C.coord.t[2]            = (s32)coord->coord.t[2];
    work->field_24C.flg                   = 0;
    work->field_4F2                       = 0;
    ctx->field_4                          = &coord->coord;
    ctx->field_48                         = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_20     = -0x190;
    ctx->node.field_4 = 0;
    ctx->field_18     = coord;
    ctx->field_1C     = 0;
    ctx->field_24     = 0;
    ctx->field_50     = &Actor01600_D09F0C;
    ctx->field_54     = (s32)&work->collision.named.pad_30C;
    ctx->field_40     = (u16)Actor01600_D09F0C.field_4;
    work->field_408   = 0x280;
    work->field_40A   = 2;
    work->field_404   = next_coord;
    func_800B3F84(&work->anim, Actor01600_D127EC, obj, work->pad_17C, work->slots);
    for (i = 1; i < 9; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_506 = 1;
    work->field_508 = 1;
    work->field_542 = 0x14;
    work->field_4EA = 0;
    work->field_52A = 0;
    work->field_52C = 0;
    work->field_4FA = 0;
    work->field_50C = 0;
    work->field_526 = 0;
    work->field_528 = 0;
    work->field_51E = 0;
    work->field_520 = 0;
    work->field_510 = 0;
    work->field_530 = 0;
    work->field_532 = 0;
    work->field_534 = 0;
    work->field_53A = 0;
    work->field_53C = 0;
    work->field_540 = 0;
    work->field_53E = 0;
    work->field_524 = 0;
    work->field_544 = 0;
    work->field_538 = 0x10;
    random          = (Gp_LcgState * 5) + 0x71357911;
    work->field_536 = (s16)(((random >> 0x10) & 0x1F) + 1);
    Gp_LcgState     = (s32)random;
    Gfx_MatrixCol2(&actor->field_2C->field_8->coord, &sp18);
    work->field_4FC = ratan2((s32)sp18.vx, (s32)sp18.vz);
    Actor01600_Fn05400(actor);
    actor->field_18 = &Actor01600_Fn06EA4;
    actor->field_24 = &Actor01600_D127A4;
    actor->field_30 = (s32)(actor->field_30 + 1);
}

void Actor01600_Fn00480(Actor01600* actor)
{
    Actor01600Work* work;
    GpRec18*        table1;
    GsCOORDINATE2*  coord;
    s8*             table2;
    s8*             table3;
    s8*             table4;

    work            = actor->field_1C;
    coord           = actor->field_2C->field_8;
    work->field_2C0 = 0xFA0;
    work->field_2CC = 0x384;
    table1          = &work->field_2D4;
    work->field_2CE = 0x64;
    work->field_2D0 = table1;
    work->field_2A8 = work->pad_2BC;
    work->field_2AC = 0;
    work->field_2AE = -0x190;
    work->field_2B0 = 0;
    work->field_2B4 = 0;
    work->field_2B8 = 0;
    work->field_2BA = 3;
    work->field_2A4 = coord;
    Gp_LinkObj(3, (struct _GpObj*)work->field_29C);
    Gp_InitRec18Table(table1, 1, 0);
    table2                          = &work->collision.field_2EC[0x20];
    work->collision.named.field_2F4 = coord;
    work->collision.named.field_2F8 = table2;
    work->collision.named.field_304 = 0x30010;
    work->collision.named.field_308 = 0x190;
    work->collision.named.field_2FC = 0;
    work->collision.named.field_2FE = -0x190;
    work->collision.named.field_300 = 0;
    work->collision.named.field_30A = 1;
    work->field_2BA                |= 0xC000;
    Gp_LinkObj(2, (struct _GpObj*)work->collision.field_2EC);
    Gp_InitRec18Table((GpRec18*)table2, 8, 0);
    table3                           = (s8*)&work->field_444;
    work->field_430                  = 0x1F4;
    work->field_42C                  = 0x1F4;
    work->field_43C                  = 1;
    work->field_43E                  = 1;
    work->field_440                  = table3;
    work->field_414                  = coord;
    work->field_418                  = (s8*)&work->field_42C;
    work->field_41C                  = 0;
    work->field_41E                  = -0x190;
    work->field_420                  = 0;
    work->field_424                  = 0;
    work->field_428                  = 0;
    work->field_42A                  = 3;
    work->collision.named.field_30A |= 0xC200;
    Gp_LinkObj(2, (struct _GpObj*)work->field_40C);
    Gp_InitRec18Table((GpRec18*)table3, 1, 0);
    table4           = work->pad_3EC;
    work->field_3D4  = coord;
    work->field_3D8  = table4;
    work->field_3DC  = 0;
    work->field_3DE  = -0x186;
    work->field_3E0  = 0;
    work->field_42A &= 0x3FFF;
    work->field_3E4  = Gp_PackPair(&Actor01600_D09F04, 1);
    work->field_3E8  = 0x12C;
    work->field_3EA  = 1;
    Gp_LinkObj(3, (struct _GpObj*)work->field_3CC);
    Gp_InitRec18Table((GpRec18*)table4, 1, 0);
    work->field_3EA &= 0x7FFF;
}

void Actor01600_Fn00480(Actor01600* arg0);
void Actor01600_Fn00A4C(Actor01600* arg0);
void Actor01600_Fn0131C(Actor01600* arg0, s32 damage);
void Actor01600_Fn00BAC(Actor01600* arg0);
void Actor01600_Fn03D48(Actor01600* arg0);
void Actor01600_Fn03EEC(Actor01600* arg0);
void Actor01600_Fn04AD8(Actor01600* arg0);
s32  Actor01600_Fn05558(Actor01600* arg0);
void Actor01600_Fn06810(Actor01600Ctx* arg0, Actor01600* arg1);
void Actor01600_Fn06A84(Actor01600* arg0);
void Actor01600_Fn06F10(Actor01600* arg0);
void Actor01600_Fn06FDC(Actor01600* arg0, s32 arg1);
s32  Gp_TickObjFlag4(Actor01600Ctx* arg0);
s32  Gp_ObjFlag4Expired(Actor01600Ctx* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_801153F4;

void Actor01600_Fn00674(Actor01600Ctx* arg0, Actor01600* arg1)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    TmdObject*      obj;
    s32             id;
    s32             map;
    u16             count;

    work  = arg1->field_1C;
    coord = arg1->field_2C->field_8;
    if (!(Actor01600_Fn05558(arg1) & 0xFF)) {
        switch (D_801153F4) {
            case 0:
                arg1->field_2C->field_C = 0;
                arg0->node.field_4      = 0;
                break;
            case 1:
                Actor01600_Fn06810(arg0, arg1);
                goto update;
            case 2:
                obj                = arg1->field_2C;
                obj->field_C      |= 0x80;
                arg0->node.field_4 = 1;
                return;
            default:
                break;
        }
        Actor01600_Fn04AD8(arg1);
        if (arg0->field_4C != 0) {
            Actor01600_Fn00A4C(arg1);
        }
        Actor01600_Fn00BAC(arg1);
        if (work->field_528 == 0) {
            if (work->field_500 != 0) {
                arg1->field_30 = 2;
            }
        }
        map = *(u32*)&gGameSession->field_4 & 0xFFFF0000;
        if (map != 0x3260000 && map != 0x4070000 && map != 0x4010000) {
            if (coord->coord.t[1] >= 0x65) {
                coord->coord.t[1] = -0xA;
            }
        }
        if (((*(u32*)&gGameSession->field_4 & 0xFFFF0000) == 0x031D0000) && (coord->coord.t[1] >= -0x3E7)) {
            id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x40100005;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            Actor01600_Fn06F10(arg1);
            Actor01600_Fn06FDC(arg1, 0);
        }
        if ((*(u32*)&gGameSession->field_4 & 0xFFFF0000) == 0x04010000) {
            if (coord->coord.t[1] > 0) {
                work->field_532 = 1;
            }
            if (coord->coord.t[1] >= 0x3E9) {
                id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x40100005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
                id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
                Actor01600_Fn06F10(arg1);
                Actor01600_Fn06FDC(arg1, 0);
            }
        }
        Actor01600_Fn01420(arg1);
        Actor01600_Fn03D48(arg1);
        Actor01600_Fn06A84(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        count           = work->field_550 + 1;
        work->field_550 = count;
        if (((s16)count >= 5) || (gGameSession->field_52 == 1)) {
            work->field_550 = 0;
            Actor01600_Fn06810(arg0, arg1);
        }
        if (work->field_532 == 0) {
        update:
            Actor01600_Fn03EEC(arg1);
        }
    }
}

void Actor01600_Fn00A4C(Actor01600* arg0)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    s16             state;
    s32             damage;
    u8              flags2;
    u8              flags1;

    ctx    = arg0->field_20;
    flags1 = ctx->field_4C;
    work   = arg0->field_1C;
    if ((flags1 & 1) && (work->field_528 == 0)) {
        ctx->field_4C   = flags1 & 0xFE;
        work->field_4FE = 2;
        work->field_538 = 0x14;
        work->field_504 = 0;
        work->field_506 = 0xE;
    }
    flags2 = ctx->field_4C;
    if ((flags2 & 2) && (work->field_4FE != 2) && (work->field_528 == 0)) {
        ctx->field_4C   = flags2 & 0xFD;
        work->field_4FE = 3;
        if (work->field_556 != 0) {
            work->field_506 = 0xE;
            work->field_4FE = 5;
        }
        work->field_504 = 0;
    }
    if ((ctx->field_4C & 0xC) && (work->field_528 == 0)) {
        Actor01600_Fn06F10(arg0);
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            state = work->field_4FE;
            if ((state != 2) && (state != 5)) {
                work->field_4FE = 0;
                work->field_506 = 0xA;
            }
            Actor01600_Fn0131C(arg0, damage);
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}

extern s32  Gp_LcgState;
extern s32* Gp_GridParams;
s16         Gp_GetIdParam2(s32);
s32         Gp_GetIdParam0(s32);
s32         Gp_GetIdParam1(s32);
s32         func_800E0C10(void*, void*, s32, void*);
u32         Gp_ComputeDamage(u32, u32, s32, s32);
s32         Gp_RollEnemyChance(void*, s32, s32);
void        func_800E2C78(void*, s32, s32, s32);
void        Gp_SetObjFlag1(void*);
void        Gp_SetObjFlag2(void*, s32, s32);
void        Gp_SetObjFlag4(void*, s32, s32);
void        Gp_ClearRec18Occupied(void*);
s32         Gp_FindRec18(void*, s32);
void        func_800FDB18(s32, void*, void*, void*);
void        Actor01600_Fn0646C(Actor01600*);

void Actor01600_Fn00BAC(Actor01600* actor)
{
    s32                    distance;
    Actor01600PlayerSlot** slots;
    void*                  world;
    Actor01600Work*        work;
    Actor01600Ctx*         ctx;
    GsCOORDINATE2*         coord;
    Actor01600Work*        rec;
    Actor01600HitScratch*  scratch;
    void*                  old;
    GsCOORDINATE2*         other;
    s32                    x, y, z;
    s32                    damage;
    s32                    amount;
    s32                    product;
    s32                    push;
    s32                    clamped;
    s32                    cx, cz;
    s16                    count;
    s32                    mode;
    VECTOR *               v1, *v2;
    work    = actor->field_1C;
    old     = *(void**)0x1F8003FC;
    scratch = (*(void**)0x1F8003FC = old - 0x4C);
    ctx     = actor->field_20;
    coord   = actor->field_2C->field_8;
    mode    = func_800E0C10(&work->collision.field_2EC[0x20], old - 0x2C, 8, old - 4);
    world   = (void*)coord + 0x50;
    if (mode == 1)
        goto mode1;
    if (mode < 2)
        goto mode_end;
    if (mode == 2)
        goto mode2;
    goto mode_end;
    {
    mode1:
        coord->coord.t[0] += scratch->delta.half.highx;
        coord->coord.t[1] += scratch->delta.half.highy;
        coord->coord.t[2] += scratch->delta.half.highz;
        goto mode_end;
    mode2:
        coord->coord.t[0] = work->field_4BC;
        coord->coord.t[1] = work->field_4C0;
        coord->coord.t[2] = work->field_4C4;
    }
mode_end:
    slots = Gp_ActorSlots;
    if (work->field_51C != 0) {
        if (--work->field_51C <= 0)
            work->field_51C = 0;
    }
    rec = work;
next_record: {
    switch (rec->collision.named.hit.parts.kind) {
        case 2:
            if (work->field_51C == 0) {
                other               = slots[rec->collision.named.hit.parts.byte0 >> 7]->field_2C->field_8;
                x                   = other->coord.t[0] - coord->coord.t[0];
                scratch->delta.v.vx = x;
                y                   = other->coord.t[1] - coord->coord.t[1];
                scratch->delta.v.vy = y;
                z                   = other->coord.t[2] - coord->coord.t[2];
                scratch->delta.v.vz = z;
                damage              = Gp_ComputeDamage(rec->collision.named.hit.id, SquareRoot0(x * x + y * y + z * z), 0, 0);
                if (Gp_RollEnemyChance(actor->field_20, rec->collision.named.hit.id, 0)) {
                    damage *= 4;
                    Gp_SpawnEff(0x6009C, (void*)actor->field_2C->field_8 + 0x50, 0, 0);
                }
                if (work->field_4FE == 1 && work->field_528 != 0 && work->field_51E < 0) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, (void*)actor->field_2C->field_8 + 0x50, 3, 0);
                }
                func_800E2C78(ctx, rec->collision.named.hit.id, damage, 0);
                Actor01600_Fn0131C(actor, damage);
                count = Gp_GetIdParam2(rec->collision.named.hit.id);
                if (count > 0)
                    work->field_51C = count;
                switch (Gp_GetIdParam0(rec->collision.named.hit.id) & 0xFFFF) {
                    case 4:
                    case 6:
                        if ((s16)ctx->field_40 <= 0)
                            goto dead;
                        Gp_SetObjFlag1(actor->field_20);
                        break;
                    case 2:
                    case 9:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->field_20, rec->collision.named.hit.id, 0);
                            work->field_556 = 1;
                        }
                        break;
                    case 8:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->field_20, rec->collision.named.hit.id, 0);
                            work->field_556 = 0;
                        }
                        break;
                    case 1:
                    case 5:
                        if (work->field_4FE != 3 && work->field_4FE != 5)
                            Gp_SetObjFlag1(actor->field_20);
                        break;
                    case 0:
                        break;
                    case 3:
                        Gp_SetObjFlag4(actor->field_20, rec->collision.named.hit.id, 0);
                        break;
                }
                if (damage >= 40 && work->field_556 == 0) {
                    Gp_SetObjFlag1(ctx);
                    if (work->field_4FE == 0 && work->field_528 != 0) {
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                    }
                } else {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x100;
                }
                if (work->field_4FE != 0 && work->field_528 != 0) {
                    work->field_3EA &= 0x7FFF;
                    Gp_ClearRec18Occupied(work->pad_3EC);
                    work->field_4FA  = 0;
                    work->field_51E += 20;
                    amount           = Actor01600_Fn045A8(actor, &distance);
                    if (amount < 0)
                        amount = -amount;
                    if (amount < 0x400) {
                        work->field_506 = 14;
                        work->field_50E = -40;
                    } else {
                        work->field_506 = 11;
                        work->field_50E = 40;
                    }
                    work->field_516 = 8;
                }
                func_800FDB18(Gp_GetIdParam1(rec->collision.named.hit.id) & 0xFFFF, world, 0, &work->pad_3EC[0x18]);
            }
            break;
        case 3:
            cx                  = coord->workm.t[0] - rec->collision.named.field_314;
            scratch->delta.v.vy = 0;
            scratch->delta.v.vx = cx;
            cz                  = coord->workm.t[2] - rec->collision.named.field_318;
            scratch->delta.v.vz = cz;
            push                = rec->collision.named.field_30E - SquareRoot0(cx * cx + cz * cz);
            clamped             = push;
            if (push <= 0)
                clamped = 0;
            push = clamped;
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            v1                  = &scratch->delta.v;
            scratch->delta.v.vx = coord->workm.t[0] - rec->collision.named.field_314;
            SOFT_TOUCH_REG_USE(push, clamped);
            v2                  = &scratch->normal;
            scratch->delta.v.vy = coord->workm.t[1] - rec->collision.named.field_316;
            scratch->delta.v.vz = coord->workm.t[2] - rec->collision.named.field_318;
            VectorNormal(v1, v2);
            ApplyTransposeMatrixLV(*Gp_GridParams + 0x24, v2, v1);
            if (work->field_506 == 23 || work->field_506 == 5 || work->field_506 == 6) {
                coord->coord.t[0] += (push * scratch->delta.v.vx) >> 12;
                product            = push * scratch->delta.v.vy;
                if (product < 0)
                    coord->coord.t[1] += product >> 12;
                coord->coord.t[2] += (push * scratch->delta.v.vz) >> 12;
            }
            break;
        case 0:
        case 1:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            break;
    }
}
    rec = (void*)rec + 0x18;
    if ((s32)rec < (s32)work + 0xC0)
        goto next_record;
    Gp_ClearRec18Occupied(&work->collision.field_2EC[0x20]);
    if (work->field_516 && Gp_FindRec18(work->pad_3EC, 0)) {
        work->collision.named.field_2FE = -400;
        work->collision.named.field_308 = 400;
        work->field_52A                 = 1;
        work->field_3EA                &= 0x7FFF;
        Gp_ClearRec18Occupied(work->pad_3EC);
        if (work->field_50A < 15) {
            work->field_516  = 8;
            work->field_4FA  = 0;
            work->field_51E += 20;
            amount           = Actor01600_Fn045A8(actor, &distance);
            if (amount < 0)
                amount = -amount;
            if (amount >= 0x400)
                goto far_angle;
            work->field_506 = 14;
            work->field_50E = -40;
            goto release;
        dead:
            work->field_540 = 0;
            Actor01600_Fn0646C(actor);
            work->field_540 = 2;
            work->field_528 = 0;
            return;
        far_angle:
            work->field_506 = 11;
            work->field_50E = 40;
        }
    }
release:
    *(void**)0x1F8003FC += 0x4C;
    return;
}

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

void Actor01600_Fn0131C(Actor01600* arg0, s32 damage)
{
    s32             id;
    s32             pan;
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  coord;

    ctx           = arg0->field_20;
    work          = arg0->field_1C;
    coord         = arg0->field_2C->field_8;
    ctx->field_40 = (u16)(ctx->field_40 - damage);
    func_800DA6E8(&ctx->node, damage, 0);
    work->field_536 = 1;
    if ((s16)ctx->field_40 <= 0) {
        work->field_500  = 1;
        work->field_502  = 0;
        work->field_3EA &= 0x7FFF;
        id               = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
    } else {
        if (work->field_506 == 1) {
            work->field_506 = 0xA;
            work->field_50A = 0;
        }
        work->field_50E = 0;
        work->field_51A = 0;
        id              = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40100009;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(coord));
    }
}

void Actor01600_Fn017BC(Actor01600* arg0);
void Actor01600_Fn020F8(Actor01600* arg0);
void Actor01600_Fn06744(Actor01600* arg0);
void Actor01600_Fn06F10(Actor01600* arg0);
s32  Gp_TickObjFlag2(void* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);

/// Per-frame tick for the actor's cornered/pursuit cycle, dispatched on
/// `field_4FE`. States 0 and 1 hand the frame to `Actor01600_Fn017BC` /
/// `Actor01600_Fn020F8` and then run the shared post-step
/// `Actor01600_Fn06744`. State 2 advances `field_504`, nudges `field_50E` back
/// by 0x3C while the animation is still 0xE and under 0x11 frames in, arms the
/// 0x16 animation at frame 0x28 and, past frame 0x5B, resets to animation 0x19
/// with the 0x8000 bit set in `field_2BA`. State 3 plays animation 0x13 until
/// `Gp_TickObjFlag2` fires. State 4 only selects animation 0x11. State 5 rolls
/// the 0x4CC swerve offset once per approach (animation 0xE, past frame 0x2C,
/// `field_522` still clear and bit 1 of `field_50A` set) and, on animation
/// 0x16 past frame 0x32, ends the cycle the same way state 2 does.
///
/// Whatever the state, animations 1/9/0x10/0x13/0x15/0x16/0x1B..0x1E are
/// silent; the rest count `field_542` down and, on expiry, play one of three
/// growls (`0x4010_0006..8`) picked by a `Gp_LcgState` draw modulo 5 - two of
/// the five outcomes stay quiet - panned and attenuated for the actor's
/// coordinate, then rearm the counter at 0x14.
void Actor01600_Fn01420(Actor01600* arg0)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    s32             id;
    s32             state;
    s32             one;
    u16             sel;
    s16             count;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;

    switch (work->field_4FE) {
        case 0:
            Actor01600_Fn017BC(arg0);
            goto tick;
        case 1:
            Actor01600_Fn020F8(arg0);
            goto tick;
        case 2:
            Actor01600_Fn06F10(arg0);
            work->field_504 = work->field_504 + 1;
            if (work->field_506 == 0xE && work->field_50A < 0x11) {
                work->field_50E = -0x3C;
                Actor01600_Fn06744(arg0);
            } else {
                work->field_50E = 0;
            }
            if (work->field_504 == 0x28) {
                work->field_538 = 0x10;
                work->field_506 = 0x16;
            }
            if (work->field_504 >= 0x5B) {
                work->field_506  = 0x19;
                work->field_4FE  = 0;
                work->field_504  = 0;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 3:
            Actor01600_Fn06F10(arg0);
            work->field_506 = 0x13;
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_4FE  = 0;
                work->field_506  = 0x19;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 4:
            work->field_506 = 0x11;
            break;
        case 5:
            Actor01600_Fn06F10(arg0);
            if (work->field_506 == 0xE) {
                if (work->field_50A >= 0x2C && work->field_522 == 0 &&
                    ((u16)work->field_50A & 2)) {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->field_506 == 0x16) {
                if (work->field_50A >= 0x32) {
                    work->field_4FE  = 0;
                    work->field_506  = 0x19;
                    work->field_2BA |= 0x8000;
                }
            }
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_506 = 0x16;
            }
            goto clear;
    }
    goto tail;

clear:
    work->field_50E = 0;
tick:
    Actor01600_Fn06744(arg0);

tail:
    state = work->field_506;
    one   = 1;
    if (state == one || state == 0x16 || state == 0x15 || state == 0x10 ||
        state == 0x13 || state == 0x1C || state == 0x1D || state == 0x1E ||
        state == 0x1B || state == 9) {
        return;
    }
    count           = work->field_542 - 1;
    work->field_542 = count;
    if (count != 0) {
        return;
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    sel         = ((u32)Gp_LcgState >> 16) % 5;
    switch (sel) {
        case 0:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100006;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
        case 1:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100007;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
        case 2:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100008;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
    }
    work->field_542 = 0x14;
}

void Actor01600_Fn017BC(Actor01600* actor)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    TmdObject*      model;
    s16             frameOffset;
    s16             count;
    s16             tick;
    s16             state;
    s16             height;
    s16             repeatHeight;
    s16             frame;
    s32             contact;
    s32             id;
    void*           old;
    s32             pan8;
    s32             distance;
    GpRec18*        rec;
    s32             pan1;
    s32             pan2;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    u16             flags;
    u16             attackFrame;

    work                = actor->field_1C;
    old                 = *(void**)0x1F8003FC;
    rec                 = &work->field_2D4;
    *(void**)0x1F8003FC = old - 8;
    model               = actor->field_2C;
    coord               = model->field_8;
    ctx                 = actor->field_20;
    if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
        work->field_51A = 1;
    }
    if (work->field_51A != 0) {
        work->field_4FE  = 1;
        work->field_516  = 0;
        work->field_510  = 0;
        work->field_4FA  = 0;
        work->field_528  = 0;
        work->field_51E  = 0;
        work->field_520  = 0;
        work->field_50A  = 0;
        work->field_4EA  = 0;
        work->field_2BA &= 0x3FFF;
        work->field_42A &= 0x3FFF;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(rec);
    if (work->field_53A == 1) {
        count           = (u16)work->field_536 - 1;
        work->field_536 = count;
        if (count == 0) {
            work->field_50A = 0;
            work->field_506 = 2;
            work->field_53A = 0;
        }
    }
    tick            = (u16)work->field_504 + 1;
    work->field_504 = tick;
    if (tick >= 0x1F) {
        work->field_504 = 0;
    }
    state = (u16)work->field_506 - 1;
    switch (state) {
        case 0:
            work->field_2C0 = 0x3E8;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if (work->field_50A >= 0x3E) {
                work->field_50A = 0;
            }
            if (ctx->field_3C->field_2 == 0) {
                if (((u8)Gp_StateF0.flags.bytes[2] & 1) || (Gp_StateF0.flags.word & 0xFF140000)) {
                    work->field_53A = 1;
                }
            }
            break;
        case 1:
            work->field_2C0 = 0xFA0;
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_50A >= 0x36) {
                work->field_506  = 0x19;
                work->field_50A  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_42A |= 0xC000;
            }
            break;
        case 24:
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            contact         = Actor01600_Fn04EB0(actor) & 0xFF;
            if (contact != 0) {
                flags           = work->field_42A & 0x3FFF;
                work->field_42A = flags;
                if (contact != 0xFF) {
                    distance        = work->field_4E0;
                    work->field_548 = 0U;
                    distance        = abs(distance);
                    work->field_50A = 0;
                    if ((distance >= 0x201) || ((contact & 0xF) == 2)) {
                        work->field_510 = 5;
                        if ((contact & 0xF0) == 0x80) {
                            work->field_506 = 7;
                        } else {
                            work->field_506 = 8;
                        }
                    } else {
                        work->field_510 = 0;
                        work->field_506 = 4;
                    }
                } else {
                    work->field_42A = flags | 0xC000;
                    work->field_4EA = 0;
                }
            }
            if (work->field_50A >= 0x57) {
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 0x19;
            }
            break;
        case 2:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x3D) {
                work->field_50A = 0;
                work->field_506 = 3;
            }
            break;
        case 3:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            if (work->field_528 != 0) {
                if (work->field_50A >= 0xC) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                height          = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = height;
                if (height >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 6) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            attackFrame = (u16)work->field_50A;
            if ((u32)(attackFrame - 5) < 0x10U) {
                if ((s16)attackFrame >= 0xC) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == 0x14) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                pan1 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan1, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A >= 0x15) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)Gp_GetObjDepth(coord));
                work->field_506 = 0x17;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_50C = 0;
                work->field_50A = 0;
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 22:
            work->field_538 = 0x10;
            if (work->field_528 != 0) {
                if (work->field_50A >= (work->field_50C + 0xC)) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                repeatHeight    = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = repeatHeight;
                if (repeatHeight >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == (work->field_50C + 6)) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            frameOffset = work->field_50C;
            frame       = work->field_50A;
            if ((frame >= (frameOffset + 6)) && ((frameOffset + 0x15) >= frame)) {
                if ((frameOffset + 0xA) >= frame) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == (work->field_50C + 0x14)) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                pan3 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan3, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A >= (work->field_50C + 0x17)) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                pan4 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan4, (s8)Gp_GetObjDepth(coord));
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_4FA = 0;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_514 = 0;
                work->field_50A = 0;
                work->field_50C = -3;
                if ((s16)work->field_548 >= 3) {
                    work->field_506 = 0x18;
                } else {
                    work->field_506 = 0x17;
                }
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 23:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            work->field_50E = 0;
            if (work->field_50A >= 0xA) {
                work->field_506  = 0x19;
                work->field_514  = 1;
                work->field_51A  = 0;
                work->field_4EA  = 0;
                work->field_42A |= 0xC000;
                work->field_2BA  = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 6:
        case 7:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_506 == 7) {
                if (work->field_50A == 0xF) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                    pan5 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan5, (s8)Gp_GetObjDepth(coord));
                }
                if (work->field_50A == 0x11) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                    pan8 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan8, (s8)Gp_GetObjDepth(coord));
                }
            } else {
                if (work->field_50A == 0xF) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                    pan6 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan6, (s8)Gp_GetObjDepth(coord));
                }
                if (work->field_50A == 0x12) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                    pan7 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan7, (s8)Gp_GetObjDepth(coord));
                }
            }
            work->field_50E = 0;
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508  = 0;
                work->field_50A  = 0;
                work->field_2BA |= 0x8000;
                if (work->field_510 == 7) {
                    work->field_514 = 0;
                    work->field_50A = 0;
                    work->field_506 = 4;
                }
            }
            break;
        case 9:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x28) {
                work->field_51A = 1;
                work->field_50A = 0;
                work->field_506 = 4;
            }
            break;
        default:
            break;
    }
    *(void**)0x1F8003FC += 8;
}

s32 Gp_DispatchMsg(Task* task, s32 message, s32 payload, s32 flags);
struct _GpObj50;
s32        Gp_PackObjPair(struct _GpObj50* ctx, s32 arg1);
extern s16 D_80072830;

void Actor01600_Fn020F8(Actor01600* actor)
{
    PlayerStatus*   config = &Player_Status;
    s32             neg_velocity;
    s32             reset_y;
    s32             reset_y2;
    SVECTOR         offset;
    s32             distance;
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  effectCoord;
    GsCOORDINATE2*  coord;
    s16             targetKind;
    s16             temp_v0_3;
    s16             temp_v0_4;
    s16             temp_v0_7;
    s16             temp_v0_8;
    s16             temp_v1;
    s16             temp_v1_10;
    s16             temp_v1_5;
    s16             temp_v1_6;
    s16             temp_v1_7;
    s32             id;
    s32             flags;
    s32             angle;
    s32             angle2;
    s32             var_v1;
    s32             pan10;
    s32             pan11;
    s32             pan12;
    s32             pan13;
    s32             pan14;
    s32             pan_case3;
    s32             pan15;
    s32             pan16;
    s32             pan17;
    s32             pan18;
    s32             pan19;
    s32             pan20;
    s32             pan21;
    s32             pan23;
    s32             pan24;
    s32             pan_msg_zero;
    s32             pan25;
    s32             pan26;
    s32             pan27;
    s32             pan28;
    s32             pan29;
    s32             pan2;
    s32             pan30;
    s32             pan31;
    s32             pan32;
    s32             pan33;
    s32             pan34;
    s32             pan35;
    s32             pan36;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    s32             pan8;
    s32             pan9;
    u16             temp_v0_6;
    u16             temp_v1_3;
    u16             temp_v1_8;
    GsCOORDINATE2*  attachedCoord;
    GsCOORDINATE2*  attachedCoord2;
    GsCOORDINATE2*  attachedOffset;
    GsCOORDINATE2*  attachedOffset2;

    ctx         = actor->field_20;
    work        = actor->field_1C;
    coord       = actor->field_2C->field_8;
    flags       = Actor01600_Fn052C4(actor) & 0xFF;
    effectCoord = &actor->field_2C->field_8[2];
    memset(&offset, 0, 8);
    offset.vy = 0x32;
    temp_v1   = work->field_516;
    switch (temp_v1) {
        case 0:
            work->field_4FA = 4;
            work->field_506 = 5;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if ((u32)((u16)work->field_50A - 9) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x12) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)Gp_GetObjDepth(coord));
                SOFT_USE_REG(work);
                if (!(Actor01600_Fn06C1C(actor) & 0xFF)) {
                    work->field_516 = 1;
                    work->field_50A = 0;
                    work->field_50E = 0;
                    work->field_506 = 0x17;
                    goto block_8;
                }
            } else {
            block_8:
                angle = Actor01600_Fn045A8(actor, &distance);
                SOFT_USE_REG(angle);
                if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                    if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                        if (work->field_50A >= 0xA) {
                            id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                            pan3 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan3, (s8)Gp_GetObjDepth(coord));
                            SOFT_USE_REG(work);
                            id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                            pan4 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan4, (s8)Gp_GetObjDepth(coord));
                            SOFT_USE_REG(work);
                            work->field_50A = 0;
                            return;
                        }
                    } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->field_50A >= 0xA)) {
                        id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                        pan5 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan5, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                        pan6 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan6, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        work->field_50A = 0;
                        return;
                    }
                } else {
                    default:
                        return;
                }
            }
            break;
        case 1:
            work->field_4FA = 4;
            work->field_538 = 0x10;
            work->field_50E = 0x1E;
            work->field_506 = 6;
            if ((u32)((u16)work->field_50A - 8) < 8U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, -0x5A);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A == 0x12) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                pan7 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan7, (s8)Gp_GetObjDepth(coord));
                SOFT_USE_REG(work);
            }
            if ((u32)((u16)work->field_50A - 0x1A) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x22) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                pan8 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan8, (s8)Gp_GetObjDepth(coord));
                work->field_50A = 0;
                if (Actor01600_Fn06C1C(actor) & 0xFF) {
                    return;
                }
            }
            angle2 = Actor01600_Fn045A8(actor, &distance);
            SOFT_USE_REG(angle2);
            if (!(Actor01600_Fn06C94(actor, angle2, distance) & 0xFF)) {
                if (Actor01600_Fn04974(actor, angle2, distance, flags) & 0xFF) {
                    if (work->field_50A >= 8) {
                        id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                        pan9 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan9, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                        pan10 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan10, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        work->field_50A = 0;
                        return;
                    }
                } else if ((Actor01600_Fn06D74(actor, angle2, distance) & 0xFF) && (work->field_50A >= 8)) {
                    id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                    pan11 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan11, (s8)Gp_GetObjDepth(coord));
                    SOFT_USE_REG(work);
                    id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                    pan12 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan12, (s8)Gp_GetObjDepth(coord));
                    SOFT_USE_REG(work);
                    work->field_50A = 0;
                    return;
                }
            }
            break;
        case 2:
            work->field_506 = 7;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                pan13 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan13, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A == 0x11) {
                var_v1 = 0x40100004;
                id     = (((u16)actor->field_20->field_8 >> 0xC) << 8) | var_v1;
                pan14  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan14, (s8)Gp_GetObjDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 3:
            work->field_506 = 8;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                pan15 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan15, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A == 0x12) {
                var_v1    = 0x40100003;
                id        = (((u16)actor->field_20->field_8 >> 0xC) << 8) | var_v1;
                pan_case3 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan_case3, (s8)Gp_GetObjDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 4:
            work->field_538 = 0x10;
            work->field_506 = 9;
            work->field_50E = 0;
            work->field_4FA = 0;
            if (work->field_50A == 1) {
                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000E;
                pan16 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan16, (s8)Gp_GetObjDepth(coord));
                SOFT_USE_REG(work);
            }
            if (work->field_50A >= 0x25) {
                work->field_516 = 0;
                work->field_50A = 0;
                work->field_526 = 0;
                return;
            }
            break;
        case 5:
            work->field_538 = 0x10;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_506 = 0x1A;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_3       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_3;
                if (temp_v0_3 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 8) {
                work->field_528 = 1;
                work->field_530 = 0;
                work->field_51E = -0x96;
                work->field_520 = (u16)work->field_520 - 0x96;
            }
            if (work->field_50A == 0x1E) {
                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                pan17 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan17, (s8)Gp_GetObjDepth(coord));
                SOFT_USE_REG(work);
                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                pan18 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan18, (s8)Gp_GetObjDepth(coord));
                SOFT_USE_REG(work);
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
            }
            temp_v1_3 = (u16)work->field_50A;
            if ((u32)(temp_v1_3 - 8) < 0x17U) {
                if (work->field_52A == 0) {
                    if ((s16)temp_v1_3 < 0xC) {
                        work->collision.named.field_2FE = -0x258;
                        work->collision.named.field_308 = 0x258;
                        work->field_50E                 = 0x12C;
                    } else if ((s16)temp_v1_3 < 0x18) {
                        work->collision.named.field_2FE = -0x190;
                        work->collision.named.field_308 = 0x190;
                        work->field_50E                 = 0x32;
                    } else {
                        work->field_50E = 0x19;
                    }
                }
                Actor01600_Fn03A60(actor);
            }
            if ((work->field_50A == 9) && (work->field_53C == 0)) {
                work->field_3EA |= 0x8000;
            }
            if (work->field_50A == 0x17) {
                work->collision.named.field_30A = (u16)(work->collision.named.field_30A | 0x4000);
                work->field_3EA                &= 0x7FFF;
            }
            if (work->field_50A >= 0x34) {
                if (work->field_53C != 0) {
                    work->field_516 = 4;
                } else {
                    work->field_516 = 0;
                }
                work->field_53C = 0;
                work->field_50A = 0;
                work->field_532 = 0;
                return;
            }
            break;
        case 6:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_4       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_4;
                if (temp_v0_4 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            temp_v1_5 = work->field_506;
            switch (temp_v1_5) {
                case 26:
                    temp_v1_6 = work->field_50A;
                    if (temp_v1_6 == 8) {
                        work->field_530 = 0;
                        work->field_528 = 1;
                        if (work->field_544 == 0) {
                            work->field_51E = -0x96;
                        } else {
                            work->field_51E = -0xC8;
                        }
                        work->field_520 = (u16)work->field_520 + (u16)work->field_51E;
                    }
                    work->field_50E = 0;
                    if (work->field_50A >= 8) {
                        if (work->field_544 == 0) {
                            if (work->field_50A < 0xC) {
                                work->collision.named.field_2FE = -0x258;
                                work->collision.named.field_308 = 0x258;
                                work->field_50E                 = 0x12C;
                            } else {
                                if (work->field_50A < 0x18) {
                                    work->collision.named.field_2FE = -0x190;
                                    work->collision.named.field_308 = 0x190;
                                    work->field_50E                 = 0x32;
                                } else {
                                    work->field_50E = 0x19;
                                }
                            }
                        } else {
                            if (work->field_50A < 0xC) {
                                work->field_50E = 0x1F4;
                            } else if (work->field_50A < 0x18) {
                                work->field_50E = 0x46;
                            } else {
                                work->field_50E = 0x32;
                            }
                        }
                    }
                    if ((work->field_50A == 9) && (work->field_53C == 0)) {
                        work->field_3EA |= 0x8000;
                    }
                    if (work->field_50A == 0x17) {
                        work->collision.named.field_30A = (u16)(work->collision.named.field_30A | 0x4000);
                        work->field_3EA                &= 0x7FFF;
                    }
                    if (work->field_50A >= 0x19) {
                        work->field_506 = 0x1B;
                        work->field_50A = 0;
                        work->field_544 = 0;
                        work->field_53C = 0;
                        return;
                    }
                    break;
                case 27:
                    work->field_50E = 0x3C;
                    if (work->field_50A == 0xB) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100005;
                        pan19 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan19, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0xD) {
                        work->field_538 = 0x10;
                        work->field_50E = 0;
                    }
                    if (work->field_50A >= 0x28) {
                        work->field_506 = 0x15;
                        work->field_50A = 0;
                    }
                    break;
                case 21:
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                        pan20 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan20, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                        pan21 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan21, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 7:
            work->field_526 = 1;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_528 = 0;
            work->field_51E = 0;
            temp_v1_7       = (u16)work->field_506 - 9;
            work->field_520 = 0;
            switch (temp_v1_7) {
                case 19:
                    Actor01600_Fn03A60(actor);
                    Actor01600_Fn06974(actor, 0xA);
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0x5A;
                    }
                    if (work->field_50A >= 0x12) {
                        if (Actor01600_Fn047A0(actor) & 0xFF) {
                            targetKind = work->field_53E;
                            if (targetKind == 1) {
                                if (D_80072830 > 0) {
                                    if (Gp_DispatchMsg(work->field_4D4, 0x3F9, Gp_PackObjPair((struct _GpObj50*)ctx, 0), 0) == targetKind) {
                                        work->field_506 = 9;
                                        work->field_50E = 0;
                                        work->field_50A = 0;
                                        work->field_4E4 = (s32)coord->coord.t[1];
                                        return;
                                    }
                                    goto block_136;
                                }
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                            if (config->hp <= 0) {
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                        block_136:
                            work->field_534 = 1;
                            work->field_4FA = 0;
                            work->field_506 = 0x1D;
                            id              = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000D;
                            pan23           = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan23, (s8)Gp_GetObjDepth(coord));
                            SOFT_USE_REG(work);
                            if (work->field_53E != 0) {
                                Actor01600_D127D8.field_4 = 1;
                                Gp_DispatchMsg(work->field_4D4, 0x3F4, (s32)&Actor01600_D127D8, 0);
                                id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4065000A;
                                pan24 = (s8)Gp_GetObjPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan24, (s8)Gp_GetObjDepth(coord));
                            } else {
                                Actor01600_D127D8.field_4 = 2;
                                Gp_DispatchMsg(work->field_4D4, 0x3FF, (s32)&Actor01600_D127D8, 0);
                                id           = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 6;
                                pan_msg_zero = (s8)Gp_GetObjPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan_msg_zero, (s8)Gp_GetObjDepth(coord));
                            }
                            work->field_554 = 2;
                            goto block_141;
                        }
                        work->field_4FA = 4;
                        work->field_526 = 0;
                        work->field_506 = 0x1B;
                    block_141:
                        work->field_50E = 0;
                        work->field_50A = 0;
                        work->field_54C = 0U;
                        return;
                    }
                    break;
                case 20:
                    attachedCoord     = ((TmdObject*)work->field_4D4->extra)->field_8;
                    attachedOffset    = &attachedCoord[17];
                    coord->coord.t[0] = attachedCoord->coord.t[0] + attachedOffset->coord.t[0];
                    coord->coord.t[2] = attachedCoord->coord.t[2] + attachedOffset->coord.t[2];
                    temp_v0_6         = work->field_54C + 1;
                    work->field_54C   = temp_v0_6;
                    if ((s16)temp_v0_6 == 0x14) {
                        work->field_54C = 0U;
                        work->field_554 = (u16)work->field_554 + 1;
                        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, effectCoord, &offset, &work->pad_3EC[0x18]);
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0x80U, 0x80U);
                        }
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000D;
                        pan25 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan25, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        if (work->field_53E == 0) {
                            Gp_DispatchMsg(work->field_4D4, 0x3F9, Gp_PackObjPair((struct _GpObj50*)ctx, 0), 0);
                        }
                        if (config->hp <= 0) {
                            if (work->field_53E == 0) {
                                work->field_506           = 9;
                                work->field_50E           = 0;
                                work->field_50A           = 0;
                                reset_y                   = coord->coord.t[1];
                                work->field_534           = 0;
                                work->field_4E4           = reset_y;
                                Actor01600_D127D8.field_4 = 0;
                                Actor01600_D127D8.field_8 = 0;
                                Actor01600_D127D8.field_C = 0;
                                Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                                return;
                            }
                            goto block_156;
                        }
                        goto block_150;
                    }
                block_150:
                    if (work->field_53E == 0) {
                        if (work->field_50A >= 0x31) {
                            if (work->field_554 >= 5) {
                                Actor01600_D127D8.field_4 = 3;
                                Actor01600_D127D8.field_8 = 1;
                                Actor01600_D127D8.field_C = 1;
                                Gp_DispatchMsg(work->field_4D4, 0x3FF, (s32)&Actor01600_D127D8, 0);
                                work->field_508 = 0;
                                work->field_50A = 0;
                                work->field_506 = 0x1E;
                            }
                            if (work->field_50A >= 0x31) {
                                work->field_50A = 0;
                                return;
                            }
                        }
                    } else {
                    block_156:
                        if (D_80072830 <= 0) {
                            work->field_506           = 9;
                            work->field_50E           = 0;
                            work->field_50A           = 0;
                            reset_y2                  = coord->coord.t[1];
                            work->field_534           = 0;
                            work->field_4E4           = reset_y2;
                            Actor01600_D127D8.field_4 = 0;
                            Actor01600_D127D8.field_8 = 0;
                            Actor01600_D127D8.field_C = 0;
                            Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                            return;
                        }
                        if (work->field_50A >= 0x31) {
                            work->field_50A = 0;
                            work->field_506 = 0x1E;
                            return;
                        }
                    }
                    break;
                case 21:
                    if (work->field_50A >= 0x10) {
                        work->field_50E = 0;
                        temp_v1_8       = (u16)work->field_50A;
                        if ((u32)(temp_v1_8 - 0x10) < 2U) {
                            work->field_50E = -0x12C;
                        } else if ((s16)temp_v1_8 < 0x1D) {
                            work->field_50E = -0x64;
                        } else if ((s16)temp_v1_8 < 0x24) {
                            work->field_50E = -0x28;
                        }
                    } else {
                        attachedCoord2    = ((TmdObject*)work->field_4D4->extra)->field_8;
                        attachedOffset2   = &attachedCoord2[17];
                        coord->coord.t[0] = attachedCoord2->coord.t[0] + attachedOffset2->coord.t[0];
                        coord->coord.t[2] = attachedCoord2->coord.t[2] + attachedOffset2->coord.t[2];
                    }
                    if (work->field_50A >= 0x2D) {
                        work->field_534           = 0;
                        Actor01600_D127D8.field_4 = 0;
                        Actor01600_D127D8.field_8 = 0;
                        Actor01600_D127D8.field_C = 0;
                        Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                    }
                    if (work->field_50A >= 0x38) {
                        work->field_506   = 9;
                        work->field_50E   = 0;
                        Actor01600_D12870 = 0;
                    }
                    if (work->field_50A == 0x13) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000B;
                        pan26 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan26, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0xD0U, 0xD0U);
                        }
                    }
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000C;
                        pan27 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan27, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                        pan28 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan28, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A == 0x21) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                        pan29 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan29, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        return;
                    }
                    break;
                case 0:
                    if (work->field_50A == 1) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4010000E;
                        pan30 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan30, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x25) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
                case 18:
                    work->field_50E = 0x28;
                    work->field_526 = 0;
                    work->field_4FA = 4;
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0;
                    }
                    if (work->field_50A == 0xD) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100005;
                        pan31 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan31, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x30) {
                        work->field_506 = 0x15;
                        work->field_50E = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 12:
                    work->field_526 = 0;
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                        pan32 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan32, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                        pan33 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan33, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_50E = 0;
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 8:
            work->field_538 = 0x14;
            work->field_4FA = 0;
            switch (work->field_506) {
                case 11:
                case 14:
                    temp_v0_7 = work->field_50A;
                    if (temp_v0_7 <= 0) {
                        work->collision.named.field_2FE = -0x258;
                        work->collision.named.field_308 = 0x258;
                        work->field_50E                 = 0x1F4;
                    } else if (temp_v0_7 < 0x13) {
                        work->collision.named.field_2FE = -0x190;
                        work->collision.named.field_308 = 0x190;
                        work->field_50E                 = 0x4B;
                    } else {
                        work->field_50E = 0;
                    }
                    if (work->field_506 == 0xE) {
                        neg_velocity    = -work->field_50E;
                        work->field_50E = neg_velocity;
                    }
                    if (work->field_528 != 0) {
                        temp_v0_8 = (u16)work->field_51E + 0xF;
                        SOFT_TOUCH_REG(temp_v0_8);
                        temp_v1_10      = (u16)work->field_520 + temp_v0_8;
                        work->field_520 = temp_v1_10;
                        work->field_51E = temp_v0_8;
                        if (temp_v1_10 >= 0) {
                            id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100005;
                            pan36 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan36, (s8)Gp_GetObjDepth(coord));
                            SOFT_USE_REG(work);
                            work->field_528 = 0;
                            work->field_520 = 0;
                        }
                    }
                    if (work->field_50A >= 0x28) {
                        if (work->field_506 == 0xE) {
                            work->field_506 = 0x16;
                        } else {
                            work->field_506 = 0x15;
                        }
                        work->field_50A = 0;
                        work->field_50E = 0;
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
                case 21:
                case 22:
                    work->field_538 = 0x10;
                    work->field_50E = 0;
                    if (work->field_50A == 0x19) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                        pan34 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan34, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A == 0x1B) {
                        id    = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                        pan35 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan35, (s8)Gp_GetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}
