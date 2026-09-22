#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

extern void* D_80067704[1];
extern u8    Actor05500_D05F18[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Actor05500_Fn0006C(Actor105500* arg0);
void Actor05500_Fn00754(Actor105500* arg0);
void Actor05500_Fn00914(Actor105500* arg0);
void Actor05500_Fn00A94(Actor105500* arg0);
void Actor05500_Fn00FA0(Actor105500* arg0);
void Actor05500_Fn012E8(Actor105500* arg0);
void Actor05500_Fn0143C(Actor105500* arg0);
void Actor05500_Fn01A0C(Actor105500* arg0);
void Actor05500_Fn01B30(Actor105500* arg0);
void ActorsSharedFn020d4(Actor105500* arg0);
void ActorsSharedFn02214(Actor105500* arg0);
void Actor05500_Fn02954(Actor105500* arg0, s32 arg1);
void ActorsSharedFn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void ActorsShared801355a4_Fn3567C(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Actor05500_Fn03B60(Actor105500* arg0);
void Actor05500_Fn03C54(Actor105500* arg0);
void Actor05500_Fn03D40(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;
void      Actor05500_Fn02FFC(GpEnemy* ctx, Task* actor)
{
    SVECTOR           rot;
    GpRec18*          rec0;
    GpRec18*          rec1;
    GpRec18*          rec2;
    GpRec18*          rec3;
    SVECTOR*          positions;
    MATRIX*           matrix;
    Actor105500Work*  work;
    s32               variant;
    s32               quotient;
    s32               i;
    s32               mode;
    GpAreaPlace*      params;
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;

    obj   = actor->extra;
    coord = obj->field_8;
    work  = memCalloc(sizeof(Actor105500Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work     = (void*)work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_1F4;
    obj->field_20   = &work->field_1D4;
    matrix          = &coord->coord;
    work->field_3C0 = 1;
    work->field_36C = &Actor05500_D08ABC;
    ctx->field_4    = matrix;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = ((Actor105500Obj2C*)actor->extra)->field_8 + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor05500_D08970;
    ctx->hp                    = (s16)Actor05500_D08970.hpMax;
    work->field_354.coord      = coord;
    work->field_354.spawnArgLo = 0x100;
    work->field_354.spawnArgHi = 1;
    work->field_3C4            = (s16)ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->variant;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(0xCC) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D089D8[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089B8;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(0xCB) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D08A10[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089F0;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    func_800B3F84((GpAnimCtx*)work, Actor05500_D08AD4, (TmdObject*)obj, work->field_154, ((Actor105500Anim*)work)->slots);
    for (i = 1; i < 8; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    rec0                     = work->field_234;
    work->field_214.coord    = coord;
    work->field_214.ctx.recs = rec0;
    work->field_214.pos.vx   = 0;
    work->field_214.pos.vy   = -0x12C;
    work->field_214.pos.vz   = 0;
    work->field_214.key      = 0x30037;
    work->field_214.radius   = 0x12C;
    work->field_214.flags    = 1;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags   |= 0x4200;
    work->field_294.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 1;
    rec1                     = work->field_2B4;
    work->field_294.ctx.recs = rec1;
    work->field_294.pos.vx   = 0;
    work->field_294.pos.vy   = -0x64;
    work->field_294.pos.vz   = 0;
    work->field_294.key      = 0x30037;
    work->field_294.radius   = 0x12C;
    work->field_294.flags    = 1;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags   |= 0x8000;
    work->field_2E4.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 4;
    rec2                     = work->field_304;
    work->field_2E4.ctx.recs = rec2;
    work->field_2E4.pos.vx   = 0;
    work->field_2E4.pos.vy   = 0;
    work->field_2E4.pos.vz   = 0;
    work->field_2E4.key      = 0;
    work->field_2E4.radius   = 0xC8;
    work->field_2E4.flags    = 1;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags   &= 0x7FFF;
    work->field_31C.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 4;
    rec3                     = work->field_33C;
    work->field_31C.ctx.recs = rec3;
    work->field_31C.pos.vx   = 0;
    work->field_31C.pos.vy   = 0;
    work->field_31C.pos.vz   = 0;
    work->field_31C.key      = 0x22424;
    work->field_31C.radius   = 0x1F4;
    work->field_31C.flags    = 1;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= 0x7FFF;
    actor->state           = 1;
}

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;
    Actor105500Work*  work;
    s32               state;
    s32               one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
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
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        ActorsSharedFn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        ActorsSharedFn020d4(arg1);
    }
    if (work->field_3A6 != 0) {
        ActorsSharedFn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}
