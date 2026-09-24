#include "common.h"

#include "actors/actor_100700.h"
#include "actors/actor_100700_spawn.h"
#include "main/tmd.h"
#include "actors/actors_shared_80135b58.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/display.h"
#include "psyq/inline_c.h"
#include "gte.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

s32  Gp_ObjFlag4Expired(Actor00700Ctx* arg0);
s32  Gp_TickObjFlag4(Actor00700Ctx* arg0);
void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

s32            Gp_TickObjFlag2(Actor00700Ctx* arg0);
void           Gp_ArmStateF0(s32 arg0);
s32            Gp_GetObjPan(void* arg0);
s32            gpGetObjDepth(GsCOORDINATE2* coord);
void           Gp_UpdateCoord(GsCOORDINATE2* arg0);
void           Gp_UnlinkObj(void* node);
void           Gp_SetLightMode(void* arg0, s32 arg1);
void           Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void           Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void           Gp_DestroyEnemy(void* enemy, void* task);
void           func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void           Gp_AnimTickIndex(void* arg0, s32 arg1);
void*          Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
extern s32     Gp_LcgState;
extern MATRIX* D_80073B8C;

void Actor00700_Fn01B50(Actor00700* arg0);
void Actor00700_Fn01EEC(Actor00700* arg0);

void Actor00700_Fn01D80(Actor00700* arg0)
{
    Actor00700Work* work2;
    s32             i;
    s32             val;

    work2 = arg0->field_1C;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
}

void Actor00700_Fn01E44(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor00700_Fn01E9C(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void Actor00700_Fn01EEC(Actor00700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

void Gp_AnimResetSlot(void*, s32, s32);
void Gp_IncStateF0Ref(s32);
void Gp_InitRec18Table(void*, s32, s32);
void Gp_LinkObj(s32, void*);
s32  Gp_PackPair(void*, s32);
void func_800B3F84(void*, void*, TmdObject*, void*, void*);

void Actor00700_Fn01FE0(Actor00700Ctx* ctx, Actor00700* actor)
{
    GsCOORDINATE2*       coord;
    TmdObject*           obj;
    s32                  i;
    void*                rec1;
    void*                rec2;
    void*                rec3;
    Actor00700SpawnWork* work;

    obj   = (TmdObject*)actor->field_2C;
    coord = obj->coords;
    work  = memCalloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = (Actor00700Work*)work;
    obj->flags      = 0;
    coord->flg      = 0;
    obj->tpage     += 1;
    obj->clut      += 1;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    obj->lightMtx = &work->field_114;
    obj->colorMtx = &work->field_F4;
    ctx->field_4  = (void*)(&coord->coord);
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_18   = coord;
    ctx->node.flags = 0;
    ctx->field_1C   = 0;
    ctx->field_20   = 0;
    ctx->field_24   = 0;
    ctx->field_50   = &Actor00700_D07588;
    ctx->field_54   = (s32)&work->field_154;
    ctx->field_40   = (u16)Actor00700_D07588.hpMax;
    work->field_228 = 0x100;
    work->field_22A = 1;
    work->field_224 = coord;
    func_800B3F84(work, &Actor00700_D075B4, obj, &work->field_B4, &work->field_14);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot(work, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_2D6 = 1;
    work->field_2AC = (s32)coord->coord.t[0];
    work->field_2B0 = (s32)coord->coord.t[1];
    work->field_2B4 = (s32)coord->coord.t[2];
    work->field_2DC = (u16)actor->field_20->field_3C->yaw;
    rec1            = &work->field_154;
    work->field_13C = coord;
    work->field_140 = rec1;
    work->field_144 = 0;
    work->field_146 = 0;
    work->field_148 = 0;
    work->field_14C = 0x30008;
    work->field_150 = 0xFA;
    work->field_152 = 1U;
    Gp_LinkObj(2, &work->field_134);
    Gp_InitRec18Table(rec1, 1, 0);
    rec2            = &work->field_18C;
    work->field_174 = coord;
    work->field_178 = rec2;
    work->field_17C = 0;
    work->field_17E = 0;
    work->field_180 = 0;
    work->field_184 = 0x30008;
    work->field_188 = 0xFA;
    work->field_18A = 1U;
    work->field_152 = (u16)(work->field_152 | 0x8000);
    Gp_LinkObj(2, &work->field_16C);
    Gp_InitRec18Table(rec2, 4, 0);
    rec3            = &work->field_20C;
    work->field_1F4 = coord;
    work->field_1F8 = rec3;
    work->field_1FC = 0;
    work->field_1FE = 0;
    work->field_200 = 0;
    work->field_18A = (u16)(work->field_18A | 0x4000);
    work->field_204 = Gp_PackPair(&Actor00700_D07584, 0);
    work->field_208 = 0x190;
    work->field_20A = 1U;
    Gp_LinkObj(3, &work->field_1EC);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_20A = (u16)(work->field_20A & 0x7FFF);
    actor->field_30 = 1;
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor00700_Fn02414(Actor00700* arg0);
void Actor00700_Fn0268C(Actor00700* arg0);
void Actor00700_Fn02820(Actor00700* arg0);
void Actor00700_Fn02A28(Actor00700* arg0);
void Actor00700_Fn03518(Actor00700* arg0);

extern u8  D_801153F4;
extern s8  D_80115408;
extern s32 Gp_LcgState;

void Actor00700_Fn02290(Actor00700Ctx* arg0, Actor00700* arg1)
{
    Actor00700Obj2C* obj;
    Actor00700Work*  work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;

    work  = arg1->field_1C;
    obj   = arg1->field_2C;
    state = D_801153F4;
    coord = obj->field_8;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C     = 0;
    arg0->node.flags = 0;
    goto default_body;
case1:
    Actor00700_Fn03518(arg1);
    return;
case2:
    obj->field_C     = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    Actor00700_Fn02414(arg1);
    Actor00700_Fn0268C(arg1);
    if (work->field_2E6 == 0 && D_80115408 != 0) {
        work->field_2E6 = 1;
        Gp_ArmStateF0(1);
    }
    Actor00700_Fn02820(arg1);
    Actor00700_Fn02A28(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Actor00700_Fn03518(arg1);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->field_8 >> 12) << 8) | 0x40070008;
        temp = (s8)Gp_GetObjPan(arg1->field_2C->field_8);
        SndEvt_EnqueueType6(id, temp, (s8)gpGetObjDepth(arg1->field_2C->field_8));
    }
}

s32  func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
u32  Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);
s32  Gp_GetIdParam1(s32 arg0);
void func_800E2C78(Actor00700Ctx* arg0, s32 arg1, s32 arg2, s32 arg3);

void Actor00700_Fn02414(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             movement;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             amount;
    s32             damage;
    s32             z;
    u16             state;
    GsCOORDINATE2*  target;
    GpDeltaScratch* head;
    GpDeltaScratch* delta;

    work     = arg0->field_1C;
    head     = *(void**)0x1F8003FC;
    delta    = (*(void**)0x1F8003FC = head - 1);
    coord    = arg0->field_2C->field_8;
    movement = func_800E0C10(&work->field_18C, delta, 4, 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].vx.h.hi;
            coord->coord.t[1] += delta->vy.h.hi;
            z                  = coord->coord.t[2] + delta->vz.h.hi;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->field_2BC;
            coord->coord.t[1] = work->field_2C0;
            coord->coord.t[2] = work->field_2C4;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_18C);
    state = (u16)work->field_154.hit.id.h.hi;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->field_30           = 2;
            arg0->field_20->field_40 = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->field_30 = (s32)state;
            target         = Gp_ActorSlots[(u8)work->field_154.hit.id.h.lo >> 7]->extra->coords;
            dx             = target->coord.t[0] - coord->coord.t[0];
            delta->vx.w    = dx;
            dy             = target->coord.t[1] - coord->coord.t[1];
            delta->vy.w    = dy;
            dz             = target->coord.t[2] - coord->coord.t[2];
            delta->vz.w    = dz;
            damage         = Gp_ComputeDamage((s32)work->field_154.hit.id.w, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount         = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&arg0->field_20->node, amount, 0);
            func_800E2C78(arg0->field_20, (s32)work->field_154.hit.id.w, damage, 0);
            arg0->field_20->field_40 = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->field_154.hit.id.w) & 0xFFFF, arg0->field_2C->field_8, 0, &work->field_224);
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154.rec);
    SCRATCH_SP += 0x10;
}

void Actor00700_Fn0268C(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  coord2;
    SVECTOR*        sc;
    s32             direction;
    s32             direction2;
    s32             product;
    sc   = (SVECTOR*)(SCRATCH_SP -= 8);
    work = arg0->field_1C;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_2D4 = !(((u32)Gp_LcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = arg0->field_2C->field_8;
    RotMatrix(sc, &coord[2].coord);
    coord[2].flg = 0;
    sc->vx       = 0;
    sc->vy       = 0;
    product      = work->field_2D8 * work->field_2D6;
    sc->vz       = -product;
    coord2       = arg0->field_2C->field_8;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].flg = 0;
    SCRATCH_SP   += 8;
}

void Actor00700_Fn02820(Actor00700* arg0)
{
    Actor00700Work*       work;
    GsCOORDINATE2*        coord;
    Actor00700RotScratch* sc;
    s32                   random;
    s32                   amount;
    s32                   cur;
    s32                   cur2;
    s32                   cur3;
    s32                   random2;
    s32                   amount2;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s16                   turn;
    s16                   wrap;

    sc    = (Actor00700RotScratch*)(SCRATCH_SP -= 0x18);
    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_2E6) {
        case 0:
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            random          = (u32)Gp_LcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->vec.vy = 0;
            sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
            diff       = want - (work->field_2DC & 0xFFF);
            adiff      = diff >= 0 ? diff : -diff;
            turn       = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    random2         = (u32)Gp_LcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void Actor00700_Fn02A28(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    u32             random;
    u32             random2;
    u32             random3;
    s32             amount;
    s32             amountB;
    s16             delta;
    s16             speed;
    s32             y;
    s32             newY;
    s16             base;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_2BC = coord->coord.t[0];
    work->field_2C0 = coord->coord.t[1];
    work->field_2C4 = coord->coord.t[2];
    switch (work->field_2E6) {
        case 0:
            random = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->field_2AC + 200 &&
                work->field_2AC - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F;
            if (work->field_2D4 != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->field_2B0 + 500 &&
                work->field_2B0 - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->field_2B4 + 200 && work->field_2B4 - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = Actor00700_D07598[arg0->field_20->field_3C->rowIndex] +
                    (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = D_80073B8C->t[1] - 0x4B0;
            random2            = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->field_2D4 != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}

void Actor00700_Fn02D28(Actor00700Ctx* arg0, Actor00700* arg1)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    SVECTOR*        head;
    SVECTOR*        rot;
    s32             angle;
    u32             rnd;
    u32             seed;
    s32             id;
    s32             pan;

    coord = arg1->field_2C->field_8;
    work  = arg1->field_1C;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            arg1->field_2C->field_C = 0x80;
            break;
        case 0:
        default:
            head                   = *(SVECTOR**)0x1F8003FC;
            rot                    = head - 1;
            *(SVECTOR**)0x1F8003FC = rot;
            switch (work->field_2DE) {
                case 0:
                    D_80115408              = 1;
                    seed                    = Gp_LcgState * 5 + 0x71357911;
                    rnd                     = seed >> 16;
                    angle                   = rnd & 0xFF;
                    arg1->field_2C->field_C = 2;
                    Gp_LcgState             = seed;
                    work->field_2E2         = 0x1000;
                    work->field_22C.matrix  = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->field_2E4                         = angle;
                    arg0->field_54                          = 0;
                    ((Actor00700SpawnWork*)work)->field_152 = ((Actor00700SpawnWork*)work)->field_152 & 0x7FFF;
                    ((Actor00700SpawnWork*)work)->field_18A = ((Actor00700SpawnWork*)work)->field_18A & 0xBFFF;
                    ((Actor00700SpawnWork*)work)->field_20A = ((Actor00700SpawnWork*)work)->field_20A | 0x8000;
                    id                                      = ((arg0->field_8 >> 12) << 8) | 0x40070006;
                    pan                                     = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(coord));
                    Gp_UnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add(arg1, 8);
                    work->field_2E0 = 1;
                    work->field_2DE = 1;
                    break;
                case 1:
                    Actor00700_Fn03570(arg1);
                    work->field_2DA = (work->field_2DA + work->field_2E4) & 0xFFF;
                    work->field_2DC = (work->field_2DC + work->field_2E4) & 0xFFF;
                    rot->vx         = work->field_2DA;
                    rot->vy         = work->field_2DC;
                    rot->vz         = 0;
                    RotMatrix(rot, &coord->coord);
                    work->field_22C.matrix.t[1] += 0x18;
                    if ((s16)(work->field_2E0 / 3) < 8) {
                        Actor00700_Fn0305C(arg1);
                    } else {
                        arg1->field_2C->field_C = 0x80;
                    }
                    work->field_2E0++;
                    if (work->field_2E0 >= 0x1E) {
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_134);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_16C);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_1EC);
                        work->field_2DE = 2;
                    }
                    break;
                case 2:
                    work->field_2E0--;
                    if (work->field_2E0 <= 0) {
                        Gp_DestroyEnemy(arg0, arg1);
                    }
                    break;
            }
            *(SVECTOR**)0x1F8003FC += 1;
            break;
    }
}

void Actor00700_Fn0305C(Actor00700* arg0)
{
    Actor00700QuadScratch* sc;
    Actor00700Work*        work;
    Actor00700Obj2C*       obj;
    GsCOORDINATE2*         coord;
    s32                    size, x, y;
    s16                    i;
    SVECTOR*               v;
    POLY_FT4*              prim;
    Actor00700TexEntry*    uv;
    obj         = arg0->field_2C;
    sc          = (Actor00700QuadScratch*)(SCRATCH_SP -= 0x28);
    coord       = obj->field_8;
    work        = arg0->field_1C;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_SP += 0x28;
        return;
    }
    if (work->field_2E0 == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->field_22C.quad.rotation);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->field_22C.quad.rotation);
        v = &sc->v[i];
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->field_24 * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->field_25 << 6) + 0x3D40;
    uv          = &Actor00700_D075BC[(s16)(work->field_2E0 / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((u_long*)(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), prim);
    SCRATCH_SP += 0x28;
}

void Actor00700_Fn034BC(Actor00700* arg0)
{
    Actor00700StateFuncTable3 sp;

    sp = Actor00700_D00054;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Actor00700_Fn03518(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor00700_Fn03570(Actor00700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_2E2 >= 0x201) {
        work->field_2E2 = (u16)work->field_2E2 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2E2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_22C.matrix;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    *(u8**)0x1F8003FC += 0x30;
}
