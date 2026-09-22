#include "common.h"

#include "actors/actors_shared_80137ca4.h"

#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

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
