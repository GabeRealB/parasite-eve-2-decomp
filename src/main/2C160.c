#include "common.h"

#include "main/game.h"

#include <psyq/inline_c.h>

#define gte_rtv0sf0()   __asm__ volatile("nop; nop; .word 0x4A406012")
#define gte_op12_real() __asm__ volatile("nop; nop; .word 0x4B78000C")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

#define gte_MulMatrix0_real(r1, r2, r3) \
    {                                   \
        gte_SetRotMatrix(r1);           \
        gte_ldclmv(r2);                 \
        gte_rtir_real();                \
        gte_stclmv(r3);                 \
        gte_ldclmv((char*)(r2) + 2);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 2);    \
        gte_ldclmv((char*)(r2) + 4);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 4);    \
    }

typedef struct {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
    /* 0x10 */ s32 lzc_min;
    /* 0x14 */ s32 lzc_tmp;
} ScratchNormBlock;

typedef struct {
    /* 0x00 */ MATRIX mat;
    /* 0x20 */ s16    sin_val;
    /* 0x22 */ s16    cos_val;
    /* 0x24 */ u8     pad[0xC];
} ScratchMat; /* 0x30 */

INCLUDE_ASM("main/nonmatchings/2C160", func_8003B960);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003BD34);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C110);

void func_8003C4F0(MATRIX* arg0, SVECTOR* arg1)
{
    void**      scratch;
    u8*         head;
    ScratchMat* block;
    s16         angle;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    block    = (ScratchMat*)(head - 0x30);
    *scratch = block;

    angle          = -ratan2(arg0->m[1][2], arg0->m[2][2]);
    arg1->vx       = angle;
    block->sin_val = rsin(angle);
    block->cos_val = rcos(arg1->vx);

    *(s16*)(head - 0x30) = ONE;
    {
        register u16     cos_u asm("v0");
        register u16     sin_u asm("v1");
        register s16     neg_s asm("v0");
        register u16     cos2 asm("v1");
        volatile MATRIX* vmat;

        cos_u         = block->cos_val;
        sin_u         = block->sin_val;
        vmat          = &block->mat;
        vmat->m[0][1] = 0;
        vmat->m[0][2] = 0;
        vmat->m[1][0] = 0;
        vmat->m[2][0] = 0;
        vmat->m[1][1] = cos_u;
        neg_s         = sin_u;
        __asm__ volatile("" : "+r"(neg_s) : "r"(sin_u));
        vmat->m[1][2] = sin_u;
        cos2          = block->cos_val;
        vmat->m[2][1] = -neg_s;
        vmat->m[2][2] = cos2;
    }

    gte_MulMatrix0_real(&block->mat, arg0, &block->mat);

    arg1->vy = ratan2(block->mat.m[0][2], block->mat.m[2][2]);
    arg1->vz = ratan2(block->mat.m[1][0], block->mat.m[1][1]);

    *scratch = (u8*)*scratch + 0x30;
}

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

void func_8003C98C(MATRIX* arg0, s32 angle, s32 flag)
{
    u8*         head;
    ScratchMat* block;
    ScratchMat* p;
    s16         cos;

    head                          = *(u8**)G_SCRATCH_HEAD;
    block                         = (ScratchMat*)(head - 0x24);
    *(ScratchMat**)G_SCRATCH_HEAD = block;
    p                             = block;

    p->sin_val = rsin(angle);
    cos        = rcos(angle);
    p->cos_val = cos;

    if (flag != 0) {
        register u16 sin_u asm("v1");
        s16          t;

        arg0->m[0][0] = cos;
        arg0->m[0][1] = 0;
        sin_u         = p->sin_val;
        arg0->m[1][0] = 0;
        arg0->m[1][1] = ONE;
        arg0->m[1][2] = 0;
        arg0->m[0][2] = sin_u;
        t             = p->sin_val;
        arg0->m[2][1] = 0;
        arg0->m[2][0] = -t;
        arg0->m[2][2] = p->cos_val;
    } else {
        register u16     sin_u asm("v1");
        register s16     neg_s asm("v0");
        register u16     cos2 asm("v1");
        volatile MATRIX* vmat;

        *(s16*)(head - 0x24) = cos;
        vmat                 = &block->mat;
        sin_u                = block->sin_val;
        vmat->m[0][1]        = 0;
        vmat->m[1][0]        = 0;
        vmat->m[1][1]        = ONE;
        vmat->m[1][2]        = 0;
        vmat->m[2][1]        = 0;
        neg_s                = sin_u;
        __asm__ volatile("" : "+r"(neg_s) : "r"(sin_u));
        vmat->m[0][2] = sin_u;
        cos2          = block->cos_val;
        vmat->m[2][0] = -neg_s;
        vmat->m[2][2] = cos2;

        gte_MulMatrix0_real(arg0, p, arg0);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x24;
    }
}

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
