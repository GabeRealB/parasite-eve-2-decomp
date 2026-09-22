#include "common.h"
#include "actors/actors_shared_80137a20.h"
#include "actors/actors_shared_80137b78.h"
#include "actors/actors_shared_80137e48.h"
#include "actors/actors_shared_80137fb0.h"
#include "actors/actors_shared_8013806c.h"
#include "actors/actors_shared_80132688.h"
#include "actors/actors_shared_80137d78.h"
#include "actors/actors_shared_80137ca4.h"
#include "main/mem.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8009EA50(s32 arg0); /* defined (s32) in src/gameplay/gameplay.c */

void ActorsShared80137a20_Fn31F54(ActorShared80137a20* arg0, ActorShared80137a20Obj2C* arg1, s32 arg2);
void ActorsShared80137a20_Fn34968(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn35BE0(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn37EF0(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn3820C(GsCOORDINATE2* arg0, s32 arg1);

extern u8 D_801153F4;

void ActorsShared80137b78_Fn329A4(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn3314C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn33AEC(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn34194(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn347F4(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn3539C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn354B0(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn35630(ActorShared80137b78* arg0);
void ActorsShared8013592c(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn35A24(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn380DC(ActorShared80137b78* arg0);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void ActorsShared80137a20(ActorShared80137a20Ctx* arg0, ActorShared80137a20* arg1)
{
    ActorShared80137a20Work*   temp_s1;
    ActorShared80137a20Obj2C*  temp_a1;
    ActorShared80137a20Coords* temp_s2;
    s32                        state;
    s32                        one;

    temp_s1 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s2 = temp_a1->field_8;
    state   = D_801153F4;
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
    if (temp_s1->field_6DA != 0) {
        temp_a1->field_C = 0;
    }
    arg0->field_14 = (temp_s1->field_49A >> 0xF) ^ 1;
    goto default_body;
case1:
    ActorsShared80137fb0((ActorShared80137fb0*)arg1);
    ActorsShared8013806c((ActorShared8013806c*)arg1);
    return;
case2:
    temp_a1->field_C = 0x80;
    arg0->field_14   = one;
    return;
default_body:
    if (temp_s1->field_6B4 != 0) {
        ActorsShared80137a20_Fn31F54(arg1, temp_a1, one);
        ActorsShared80137b78((ActorShared80137b78*)arg1);
        ActorsShared80137e48((ActorShared80137e48*)arg1);
        ActorsShared80137a20_Fn37EF0(arg1);
        ActorsShared80137a20_Fn35BE0(arg1);
        temp_s2->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_F0.flg = 0;
        Gp_UpdateCoord(&temp_s2->field_0);
        ActorsShared80137fb0((ActorShared80137fb0*)arg1);
        ActorsShared8013806c((ActorShared8013806c*)arg1);
        ActorsShared80137a20_Fn3820C(&arg1->field_2C->field_8->field_F0, 0xC);
        ActorsShared80137a20_Fn34968(arg1);
        func_8009EA50(temp_s1->field_6D8);
    }
}

void ActorsShared80137b78(ActorShared80137b78* arg0)
{
    s16                      temp_v1;
    ActorShared80137b78Work* temp_s1;

    temp_s1 = arg0->field_1C;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            ActorsShared80137b78_Fn329A4(arg0);
            break;
        case 1:
            ActorsShared80137b78_Fn3314C(arg0);
            break;
        case 2:
            ActorsShared80137b78_Fn33AEC(arg0);
            break;
        case 3:
            ActorsShared80137b78_Fn34194(arg0);
            break;
        case 4:
            ActorsShared80137b78_Fn347F4(arg0);
            break;
        case 5:
            ActorsShared80137b78_Fn3539C(arg0);
            break;
        case 6:
            ActorsShared80137b78_Fn354B0(arg0);
            break;
        case 7:
            ActorsShared80137b78_Fn35630(arg0);
            break;
        case 8:
            ActorsShared8013592c(arg0);
            break;
        case 9:
            ActorsShared80137b78_Fn35A24(arg0);
            break;
        case 10:
            ActorsShared80137d78((ActorShared80137d78*)arg0);
            break;
        case 11:
            ActorsShared80132688((ActorShared80132688*)arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        ActorsShared80137b78_Fn380DC(arg0);
    }
}

void ActorsShared80137ca4(ActorShared80137ca4* arg0)
{
    void**                   scratch;
    void*                    head;
    ActorShared80137ca4Mat*  m;
    GsCOORDINATE2*           coord;
    ActorShared80137ca4Work* work;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    m        = (ActorShared80137ca4Mat*)((u8*)head - 0x20);
    *scratch = m;
    coord    = arg0->field_2C->field_8;
    work     = arg0->field_1C;

    coord->coord     = work->field_674;
    m->ident.m00_m01 = 0x1000;
    m->ident.m02_m10 = 0;
    m->ident.m11_m12 = 0x1000;
    m->ident.m20_m21 = 0;
    m->ident.m22     = 0x1000;
    ScaleMatrix(&m->mat, &work->field_694);
    MulMatrix(&coord->coord, &m->mat);
    *scratch = (u8*)*scratch + 0x20;
}

void ActorsShared80137d78(ActorShared80137d78* arg0)
{
    ActorShared80137d78Work* work;
    s16                      state;
    s32                      next;

    work  = arg0->field_1C;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0xA;
            work->field_6DE = 5;
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                arg0->field_30  = 2;
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                arg0->field_30  = state;
                work->field_6CE = 0;
            }
            break;
    }
}

void ActorsShared80137e48(ActorShared80137e48* arg0)
{
    ActorShared80137e48Work* work;
    GsCOORDINATE2*           coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_664    = coord->coord.t[0];
    work->field_668    = coord->coord.t[1];
    work->field_66C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_6C8) >> 12;
    if (work->field_714 < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_6C8) >> 12;
}
