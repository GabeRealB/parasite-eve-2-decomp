#include "common.h"

#include "main/game.h"

#include <psyq/inline_c.h>

#define gte_rtv0sf0()   __asm__ volatile("nop; nop; .word 0x4A406012")
#define gte_op12_real() __asm__ volatile("nop; nop; .word 0x4B78000C")

typedef struct {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
    /* 0x10 */ s32 lzc_min;
    /* 0x14 */ s32 lzc_tmp;
} ScratchNormBlock;

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

void func_8003CD78(VECTOR* light, SVECTOR* out)
{
    register void**            scratch asm("s0");
    register u8*               head asm("a1");
    register ScratchNormBlock* block asm("a2");
    register ScratchNormBlock* vec asm("a0");
    register s32*              p_min asm("v0");
    register s32*              p_tmp asm("a3");
    register s32               val asm("a0");
    s32                        shift;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    block    = (ScratchNormBlock*)(head - 0x18);
    *scratch = block;

    *(VECTOR*)(head - 0x18) = *light;

    vec = block;

    gte_ldlzc(vec->vx);
    gte_nop();
    gte_nop();
    p_min = (s32*)(head - 8);
    gte_stlzc(p_min);

    gte_ldlzc(vec->vy);
    gte_nop();
    gte_nop();
    p_tmp = (s32*)(head - 4);
    gte_stlzc(p_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    val = vec->vz;
    gte_ldlzc(val);
    gte_nop();
    gte_nop();
    gte_stlzc(p_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    val = block->lzc_min;
    if (val < 0x12) {
        shift                  = 0x12 - val;
        block->lzc_min         = shift;
        *(s32*)(head - 0x18) >>= shift;
        {
            register s32 t_vy asm("v0");
            register s32 t_sh asm("a0");
            register s32 t_vz asm("v1");
            register s32 t_sh2 asm("a1");

            t_vy = block->vy;
            __asm__ volatile("" ::"r"(t_vy));
            t_sh  = block->lzc_min;
            t_vz  = block->vz;
            t_sh2 = t_sh;
            __asm__ volatile("" : "+r"(t_sh2));
            block->vy = t_vy >> t_sh;
            block->vz = t_vz >> t_sh2;
        }
    }

    VectorNormalS((VECTOR*)block, out);

    *scratch = (u8*)*scratch + 0x18;
}

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
