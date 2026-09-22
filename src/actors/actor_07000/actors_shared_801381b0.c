#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_801381b0.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

void ActorsShared801381b0(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                  = (Actor107000Spawn2Work*)arg0->work;
    head                  = *(u8**)0x1F8003FC;
    *(MATRIX**)0x1F8003FC = (MATRIX*)(head - 0x20);
    scratch               = (MATRIX*)(head - 0x20);
    coord                 = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    *(MATRIX**)0x1F8003FC = (MATRIX*)((u8*)*(MATRIX**)0x1F8003FC + 0x20);
}
