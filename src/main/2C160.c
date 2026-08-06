#include "common.h"

#include "main/game.h"

#include <psyq/inline_c.h>

#define gte_rtv0sf0()   __asm__ volatile("nop; nop; .word 0x4A406012")
#define gte_op12_real() __asm__ volatile("nop; nop; .word 0x4B78000C")

INCLUDE_ASM("main/nonmatchings/2C160", func_8003B960);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003BD34);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C110);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C4F0);

void func_8003C6D8(MATRIX* arg0, volatile MATRIX* arg1)
{
    register short t4 asm("t4");
    register short t5 asm("t5");
    register short t6 asm("t6");

    t4            = arg0->m[0][0];
    t5            = arg0->m[1][0];
    t6            = arg0->m[2][0];
    arg1->m[0][0] = t4;
    arg1->m[0][1] = t5;
    arg1->m[0][2] = t6;

    t4            = arg0->m[0][1];
    t5            = arg0->m[1][1];
    t6            = arg0->m[2][1];
    arg1->m[1][0] = t4;
    arg1->m[1][1] = t5;
    arg1->m[1][2] = t6;

    t4            = arg0->m[0][2];
    t5            = arg0->m[1][2];
    t6            = arg0->m[2][2];
    arg1->m[2][0] = t4;
    arg1->m[2][1] = t5;
    arg1->m[2][2] = t6;
}

void func_8003C728(MATRIX* arg0, volatile SVECTOR* arg1)
{
    register short t4 asm("t4");
    register short t5 asm("t5");
    register short t6 asm("t6");

    t4       = arg0->m[0][0];
    t5       = arg0->m[1][0];
    t6       = arg0->m[2][0];
    arg1->vx = t4;
    arg1->vy = t5;
    arg1->vz = t6;
}

void func_8003C748(MATRIX* arg0, volatile SVECTOR* arg1)
{
    register short t4 asm("t4");
    register short t5 asm("t5");
    register short t6 asm("t6");

    t4       = arg0->m[0][1];
    t5       = arg0->m[1][1];
    t6       = arg0->m[2][1];
    arg1->vx = t4;
    arg1->vy = t5;
    arg1->vz = t6;
}

void func_8003C768(MATRIX* arg0, volatile SVECTOR* arg1)
{
    register short t4 asm("t4");
    register short t5 asm("t5");
    register short t6 asm("t6");

    t4       = arg0->m[0][2];
    t5       = arg0->m[1][2];
    t6       = arg0->m[2][2];
    arg1->vx = t4;
    arg1->vy = t5;
    arg1->vz = t6;
}

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C788);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C98C);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003CB80);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003CD78);

void func_8003CEC4(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2)
{
    register void**   scratch asm("s1");
    register u8*      head asm("v1");
    register SVECTOR* sv1 asm("a0");
    MATRIX*           mat;
    volatile MATRIX*  dest;
    u16               tmp;
    register short    t4 asm("t4");
    register short    t5 asm("t5");
    register short    t6 asm("t6");

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch;
    dest    = out;

    *(GBytes8*)(head - 0x1A) = *(GBytes8*)arg2;

    sv1      = (SVECTOR*)(head - 0x14);
    tmp      = arg1->vx;
    sv1->vx  = tmp;
    mat      = (MATRIX*)(head - 0x20);
    tmp      = arg1->vy;
    sv1->vy  = tmp;
    tmp      = arg1->vz;
    head     = head - 0x1A;
    *scratch = mat;
    sv1->vz  = tmp;

    gte_ldopv1SV(head);
    gte_ldopv2SV(sv1);
    gte_op12_real();
    gte_stsv(mat);

    MatrixNormal_2(mat, mat);

    t4            = mat->m[0][0];
    t5            = mat->m[1][0];
    t6            = mat->m[2][0];
    dest->m[0][0] = t4;
    dest->m[0][1] = t5;
    dest->m[0][2] = t6;

    t4            = mat->m[0][1];
    t5            = mat->m[1][1];
    t6            = mat->m[2][1];
    dest->m[1][0] = t4;
    dest->m[1][1] = t5;
    dest->m[1][2] = t6;

    t4            = mat->m[0][2];
    t5            = mat->m[1][2];
    t6            = mat->m[2][2];
    dest->m[2][0] = t4;
    dest->m[2][1] = t5;
    dest->m[2][2] = t6;

    *scratch = (u8*)*scratch + 0x20;
}

s32 func_8003D000(SVECTOR* arg0, SVECTOR* arg1)
{
    s32 result;

    gte_ldsvrtrow0(arg0);
    gte_ldv0(arg1);
    gte_rtv0sf0();
    gte_stlvnl0(&result);
    return result;
}
