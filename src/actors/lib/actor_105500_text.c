#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"

#include "main/mem.h"

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
void Actor05500_Fn020D4(Actor105500* arg0);
void Actor05500_Fn02214(Actor105500* arg0);
void Actor05500_Fn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void Actor05500_Fn03864(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0006C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00754);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00A94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00FA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn012E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0143C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01A0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01B30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn020D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02214);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02364);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02780);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02954);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02FFC);

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
        Actor05500_Fn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        Actor05500_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor05500_Fn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03674);

void Actor05500_Fn0378C(Actor105500* arg0)
{
    s16 state;

    state = arg0->field_1C->field_39A;
    switch (state) {
        case 0:
            Actor05500_Fn00754(arg0);
            break;
        case 1:
            Actor05500_Fn00914(arg0);
            break;
        case 2:
            Actor05500_Fn00A94(arg0);
            break;
        case 3:
            Actor05500_Fn00FA0(arg0);
            break;
        case 4:
            Actor05500_Fn012E8(arg0);
            break;
        case 5:
            Actor05500_Fn0143C(arg0);
            break;
        case 6:
            Actor05500_Fn01A0C(arg0);
            break;
        case 7:
            Actor05500_Fn03864(arg0);
            break;
        case 8:
            Actor05500_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03864);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03918);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn039AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03A70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03AC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03B60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03C54);

/// Folds a uniform 1/16 scale into the model's third coordinate node, through a
/// 0x30-byte block borrowed from the scratchpad and released again: an identity
/// rotation is splatted word-wise, `ScaleMatrix` shrinks its diagonal to 0x100,
/// and `MulMatrix` multiplies the result into `field_8[2].coord`. This is the
/// body shared as `ActorsShared80135b58`, which this whole-overlay unit is too
/// coarse to link against.
void Actor05500_Fn03D40(Actor105500* actor)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135b58Scratch* blk;
    GsCOORDINATE2*              coord;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *scratch = blk;
    coord    = actor->field_2C->field_8;

    blk->scale.vx          = 0x100;
    blk->scale.vy          = 0x100;
    blk->scale.vz          = 0x100;
    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;
    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord[2].coord, &blk->mat.mat);
    *scratch = (u8*)*scratch + 0x30;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03DD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03E34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03F88);
