#include "common.h"

#include "actors/actor_102500.h"
#include "actors/actors_shared_80135b58.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/gfx.h"
#include "main/wipsys.h"
#include "main/mem.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s16 Actor02500_D05BD0[];
extern s16 Actor02500_D05B48[];
extern s16 Actor02500_D05B58[];

void Gp_AnimTickIndex(Actor02500Work* arg0, s32 arg1);
void func_800B4114(Actor02500Work* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_DestroyEnemy(Actor02500Ctx* ctx, Actor02500* actor);
void func_800B3F84(Actor02500Work* arg0, void* arg1, Actor02500Obj2C* arg2, void* arg3,
                   Actor02500AnimSlots* arg4);
void Gp_AnimResetSlot(Actor02500Work* arg0, s32 arg1, s32 arg2);
void Gp_IncStateF0Ref(s32 arg0);
void Gp_SetLightMode(Actor02500Ctx* arg0, s32 arg1);
void Gp_LinkObj(s32 arg0, GpObj* arg1);
void Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2);
s32  Gp_PackPair(void* pairs, s32 index);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void Actor02500_Fn016FC(Actor02500* arg0)
{
    Actor02500Work*       work;
    GsCOORDINATE2*        coord;
    Actor02500RotScratch* sc;
    s32                   ang;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s32                   step;
    s32                   cur;
    s32                   next;
    s32                   wrapStep;

    sc    = (Actor02500RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_32A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_32C = ang;
    if (adiff < 0x800) {
        step = work->field_328;
        if (step >= adiff) {
            work->field_32C = want;
        } else {
            next = work->field_32C;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_32C = next;
        }
    } else {
        step = work->field_328;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_32C = work->field_32A;
        goto done;
    turn:
        wrapStep = work->field_328;
        cur      = work->field_32C;
        if (diff > 0) {
            work->field_32C = cur - wrapStep;
        } else {
            work->field_32C = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_32C;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

extern char  Actor02500_D04448;
extern char  Actor02500_D0478C;
extern char  Actor02500_D04AD0;
extern void* D_80067704[1];

void*                Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
Actor02500AreaTable* Gp_GetNestedAreaRec(GpAreaKey* key);
void                 Gp_SyncAreaKeyIndex(GpAreaKey* key);

void Actor02500_Fn0184C(Actor02500* arg0)
{
    GpAreaKey          key;
    u32                raw1, raw2, raw3;
    u8                 areaByte0;
    TmdObject*         model1;
    TmdObject*         model2;
    TmdObject*         model3;
    u32                index1;
    u32                index2;
    u32                index3;
    Actor02500Eff*     effect1;
    Actor02500Eff*     effect2;
    Actor02500Eff*     effect3;
    Actor02500AreaRec* entry1;
    Actor02500AreaRec* entry2;
    Actor02500AreaRec* entry3;
    GpAreaKey*         sessionKey1;
    GpAreaKey*         sessionKey2;
    GpAreaKey*         sessionKey3;

    D_80067704[0] = &Actor02500_D04448;
    effect1       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->at4.loc;
        raw1        = arg0->field_20->field_8;
        model1      = (TmdObject*)effect1->task->field_2C;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->at4.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1        = (Actor02500AreaRec*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->field_D;
        model1->clut  = entry1->field_E;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }
    D_80067704[0] = &Actor02500_D0478C;
    effect2       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->at4.loc;
        raw2        = arg0->field_20->field_8;
        model2      = (TmdObject*)effect2->task->field_2C;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (Actor02500AreaRec*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->field_D;
        model2->clut  = entry2->field_E;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }
    D_80067704[0] = &Actor02500_D04AD0;
    effect3       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->at4.loc;
        raw3        = arg0->field_20->field_8;
        model3      = (TmdObject*)effect3->task->field_2C;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (Actor02500AreaRec*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->field_D;
        model3->clut  = entry3->field_E;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }
}

void  Gp_UnlinkObj(void* node);
void  Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void  Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
void* Gp_SpawnEnemyFromTable(void* table, s32 idx, s32 arg2, void* parent);
void  Actor02500_Fn0184C(Actor02500* arg0);
void  Actor02500_Fn02480(Actor02500* arg0);

extern u8 D_801153F4;

void Actor02500_Fn01AC8(Actor02500Ctx* arg0, Actor02500* arg1)
{
    Actor02500Work*  work;
    Actor02500Obj2C* obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           vec;
    s32              mode;
    s32              one;
    s16              st;
    s16              phase;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    mode  = D_801153F4;
    coord = obj->field_8;
    if (mode == 1) {
        goto case1;
    }
    if (mode < 2) {
        goto common;
    }
    if (mode == 2) {
        goto case2;
    }
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C = 0x80;
    return;
common:
    one = 1;
    st  = work->field_324;
    if (st == one) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    if (st == 3) {
        goto case3;
    }
    return;
death:
    work->field_31C = 8;
    work->field_32E = 0;
    work->field_332 = 0x1000;
    work->field_2E4 = coord->coord;
    arg0->field_54  = NULL;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->obj16C);
    Gp_UnlinkObj(&work->obj1A4);
    Gp_UnlinkObj(&work->obj20C);
    Gp_UnlinkObj(&work->obj2A4);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 0x19);
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    if (work->field_33C == 0) {
        work->field_324 = one;
        return;
    }
    obj->field_C    = 0x80;
    work->field_324 = 3;
    return;
dying:
    Actor02500_Fn02480(arg1);
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if (work->field_32E == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
        Gp_SpawnEnemyFromTable(Actor02500_D05B88, 1, 0, arg0);
    }
    if (work->field_32E >= 0x3C) {
        obj->field_C    = 0x80;
        work->field_324 = 2;
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
case3:
    if (work->field_33C == 0) {
        goto timer;
    }
    if (work->field_33C < 2) {
        goto inc;
    }
    work->field_33C = 0;
    Tmd_FreeBuffers((TmdObject*)obj);
    obj->field_C |= 4;
    Actor02500_Fn0184C(arg1);
    goto timer;
inc:
    work->field_33C++;
timer:
    phase           = work->field_32E + 1;
    work->field_32E = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_324 = 2;
}

void Actor02500_Fn01E04(Actor02500* arg0)
{
    Actor02500StateFuncTable3 sp;

    sp = Actor02500_D00004;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor02500_Fn00494(Actor02500* arg0);
void Actor02500_Fn016FC(Actor02500* arg0);
void Actor02500_Fn02008(Actor02500* arg0);
void Actor02500_Fn02288(Actor02500* arg0);
void Actor02500_Fn02318(Actor02500* arg0);
void Actor02500_Fn023D8(Actor02500* arg0);
void Actor02500_Fn02430(Actor02500* arg0);

void Actor02500_Fn01E60(Actor02500Ctx* arg0, Actor02500* arg1)
{
    Actor02500Work*  work;
    Actor02500Obj2C* temp_a1;
    GsCOORDINATE2*   temp_s2;
    s32              state;
    s32              one;

    temp_a1 = arg1->field_2C;
    state   = D_801153F4;
    work    = arg1->field_1C;
    temp_s2 = temp_a1->field_8;
    one     = 1;
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
    temp_a1->field_C = 0;
    arg0->node.flags = 0;
    goto default_body;
case1:
    if (work->field_322 == 5) {
        return;
    }
    Actor02500_Fn023D8(arg1);
    goto tail;
case2:
    temp_a1->field_C = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor02500_Fn01F8C(arg1);
    }
    Actor02500_Fn00494(arg1);
    Actor02500_Fn02008(arg1);
    if (work->field_328 != 0) {
        Actor02500_Fn016FC(arg1);
    }
    Actor02500_Fn02288(arg1);
    Actor02500_Fn02318(arg1);
    temp_s2->flg = 0;
    Gp_UpdateCoord(temp_s2);
    Actor02500_Fn023D8(arg1);
    if (work->field_322 == 5) {
        return;
    }
tail:
    Actor02500_Fn02430(arg1);
}

void Actor02500_Fn01F8C(Actor02500* actor)
{
    u8              flags;
    u8              remainingFlags;
    Actor02500Work* work;
    Actor02500Ctx*  ctx;

    ctx   = actor->field_20;
    flags = ctx->field_4C;
    work  = actor->field_1C;
    if (flags & 2) {
        ctx->field_4C   = (u8)(flags & 0xFD);
        work->field_322 = 4;
        work->field_324 = 0;
    }
    if (ctx->field_4C & 1) {
        ctx->field_4C = (u8)(ctx->field_4C & 0xFE);
        if (work->field_322 != 4) {
            work->field_322 = 3;
            work->field_324 = 0;
        }
    }
    remainingFlags = ctx->field_4C;
    if (remainingFlags & 0xC) {
        ctx->field_4C = (u8)(remainingFlags & 0xF3);
    }
}

void Actor02500_Fn00B18(Actor02500* arg0);
void Actor02500_Fn00DD8(Actor02500* arg0);
void Actor02500_Fn01144(Actor02500* arg0);
void Actor02500_Fn012F0(Actor02500* arg0);
void Actor02500_Fn020D0(Actor02500* arg0);
void Actor02500_Fn02178(Actor02500* arg0);
void Actor02500_Fn021F8(Actor02500* arg0);

void Actor02500_Fn02008(Actor02500* arg0)
{
    switch (arg0->field_1C->field_322) {
        case 0:
            Actor02500_Fn00B18(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 1:
            Actor02500_Fn00DD8(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case 2:
            Actor02500_Fn020D0(arg0);
            break;
        case 3:
            Actor02500_Fn02178(arg0);
            break;
        case 4:
            Actor02500_Fn021F8(arg0);
            break;
        case 5:
            Actor02500_Fn012F0(arg0);
            break;
        case 6:
            arg0->field_30 = 2;
            break;
    }
}

void Actor02500_Fn020D0(Actor02500* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->field_1C;
    state = work->field_324;
    switch (state) {
        case 0:
            work->field_31C = 7;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_324 = 1;
            return;
        case 1:
            if ((s16)work->field_320 >= 0x20) {
                if (work->field_33E == state) {
                    work->field_322 = 4;
                    work->field_324 = 0;
                    return;
                }
                if (work->field_344 == state) {
                    work->field_322 = 3;
                    work->field_324 = state;
                    work->field_32E = 0x3C;
                    return;
                }
                work->field_322 = state;
                work->field_324 = 0;
            } else {
                return;
            }
            break;
    }
}

void Actor02500_Fn02178(Actor02500* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->field_1C;
    state = work->field_324;

    switch (state) {
        case 0:
            work->field_31C = 7;
            work->field_344 = 1;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_32E = 0x3C;
            work->field_324 = 1;
            break;
        case 1:
            if (--work->field_32E <= 0) {
                work->field_322 = state;
                work->field_324 = 0;
                work->field_344 = 0;
            }
            break;
    }
}

s32 Gp_TickObjFlag2(Actor02500Ctx* arg0);

void Actor02500_Fn021F8(Actor02500* arg0)
{
    Actor02500Work* work;
    s32             state;

    work  = arg0->field_1C;
    state = work->field_324;

    switch (state) {
        case 0:
            work->field_31C = 0xB;
            work->field_33E = 1;
            work->field_344 = 0;
            work->field_326 = 0;
            work->field_328 = 0;
            work->field_324 = 1;
            break;
        case 1:
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_322 = state;
                work->field_324 = 0;
                work->field_33E = 0;
            }
            break;
    }
}

void Actor02500_Fn02288(Actor02500* arg0)
{
    Actor02500Work* work;
    GsCOORDINATE2*  coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_304    = coord->coord.t[0];
    work->field_308    = coord->coord.t[1];
    work->field_30C    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_326) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_326) >> 0xC;
}

void Actor02500_Fn02318(Actor02500* arg0)
{
    Actor02500Work* work;
    s16             anim;
    s32             value;
    s32             i;
    s32             j;

    work = arg0->field_1C;
    anim = work->field_31C;
    if (anim != work->field_31E) {
        value           = Actor02500_D05BD0[anim];
        i               = 1;
        work->field_31E = work->field_31C;
        work->field_320 = 0;
        do {
            func_800B4114(work, i, work->field_31C, 0, value);
            i++;
        } while (i < 5);
        return;
    }
    j = 1;
    work->field_320++;
    do {
        Gp_AnimTickIndex(work, j);
        j++;
    } while (j < 5);
}

void Actor02500_Fn023D8(Actor02500* arg0)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor02500_Fn02430(Actor02500* arg0)
{
    VECTOR3        vec;
    GsCOORDINATE2* coord;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
}

void Actor02500_Fn02480(Actor02500* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor02500Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_332 >= 0x201) {
        work->field_332 = (u16)work->field_332 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_332;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_2E4;
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

void Actor02500_Fn02574(Actor02500* arg0)
{
    Actor02500StateFuncTable3 sp;

    sp = Actor02500_D00050;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Actor02500_Fn025D0(Actor02500Ctx* ctx, Actor02500* task)
{
    Actor02500EffWork* work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GsCOORDINATE2*     parentCoord;
    void*              effect;

    coord       = task->field_2C->field_8;
    parentCoord = task->parent->field_2C->field_8;
    work        = memCalloc(0x40, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, task);
        return;
    }
    task->field_1C     = (Actor02500Work*)work;
    coord->sub         = &gGfxViewCoord;
    coord->coord       = parentCoord->coord;
    coord->coord.t[0]  = parentCoord->coord.t[0];
    coord->coord.t[1]  = parentCoord->coord.t[1];
    coord->coord.t[2]  = parentCoord->coord.t[2];
    coord->flg         = 0;
    effect             = Gp_SpawnEff(0x80060046, coord, 0x10280, NULL);
    work->obj.coord    = coord;
    rec                = work->rec18;
    work->field_38     = effect;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.key      = Gp_PackPair(&Actor02500_D05B30, 1);
    work->obj.radius   = 0xC8;
    work->obj.flags    = 1U;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 1, 0);
    work->obj.flags = (u16)(work->obj.flags | 0x8000);
    Task_DetachFromParent(task);
    task->field_30 = 1;
}

void Actor02500_Fn02750(Actor02500Ctx* ctx, Actor02500* task)
{
    s32                sound;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    s32                done;
    s32                pan;
    u16                timer;
    Actor02500EffWork* work;

    coord = task->field_2C->field_8;
    work  = (Actor02500EffWork*)task->field_1C;
    done  = 0;
    if (Gp_StateF0.field_4 == 0) {
        rec = work->rec18;
        if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
            done  = 1;
            sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40190007;
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(coord));
        }
        Gp_ClearRec18Occupied(rec);
        timer          = work->field_3C + 1;
        work->field_3C = timer;
        if ((s16)timer >= 0xF1) {
            done = 1;
        }
        if (Gp_StateF0.field_6 == 0) {
            done = 1;
        }
        if (done != 0) {
            work->field_3E = 0;
            task->field_30 = 2;
        }
    }
}

void Actor02500_Fn02874(Actor02500Ctx* ctx, Actor02500* task)
{
    Actor02500EffWork* work = (Actor02500EffWork*)task->field_1C;

    switch (work->field_3E) {
        case 0:
            Gp_UnlinkObj(&work->obj);
            if (work->field_38 != NULL) {
                work->field_38->task->field_30 = 3;
            }
            work->field_3C = 0x1E;
            work->field_3E = 1;
            break;
        case 1:
            if (--work->field_3C > 0) {
                break;
            }
            Gp_DestroyEnemy(ctx, task);
            break;
    }
}
