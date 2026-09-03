#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"

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

typedef struct {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ s16     sin_x;
    /* 0x22 */ s16     cos_x;
    /* 0x24 */ s16     sin_y;
    /* 0x26 */ s16     cos_y;
    /* 0x28 */ s16     sin_z;
    /* 0x2A */ s16     cos_z;
    /* 0x2C */ SVECTOR vec;
} ScratchRotXYZ; /* 0x34 */

typedef struct {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ s16     sin_x;
    /* 0x22 */ s16     cos_x;
    /* 0x24 */ s16     sin_y;
    /* 0x26 */ s16     cos_y;
    /* 0x28 */ s16     sin_z;
    /* 0x2A */ s16     cos_z;
    /* 0x2C */ SVECTOR vec;
    /* 0x34 */ SVECTOR vec2;
    /* 0x3C */ SVECTOR vec3;
} ScratchRotZYX; /* 0x44 */

void Gfx_RotMatrixXYZ(MATRIX* out, SVECTOR* angles, s32 flag)
{
    void**                  s;
    u8*                     head;
    register void*          p asm("v0");
    ScratchRotXYZ*          block;
    SVECTOR*                vec;
    volatile ScratchRotXYZ* vblock;
    volatile MATRIX*        vmat;
    u16                     cos_u;
    register u16            cos_x2 asm("a1");
    u16                     cos_y;
    register s16            sin_y asm("v1");
    register u16            sin_x asm("v0");
    register s16            neg asm("v0");
    register s16            sin_copy asm("a0");
    void*                   col1;
    void*                   col2;

    s     = (void**)G_SCRATCH_HEAD;
    head  = (u8*)*s;
    p     = head - 0x34;
    block = p;
    *s    = p;

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    *(s16*)(head - 0x34) = ONE;
    vblock               = block;
    {
        cos_u         = vblock->cos_x;
        cos_x2        = vblock->cos_x;
        cos_y         = vblock->cos_y;
        sin_y         = vblock->sin_y;
        vmat          = &block->mat;
        vmat->m[0][1] = 0;
        vmat->m[0][2] = 0;
        vmat->m[1][0] = 0;
        vmat->m[2][0] = 0;
        block->vec.vy = 0;
        vmat->m[1][1] = cos_u;
        sin_x         = vblock->sin_x;
        sin_y         = -sin_y;
        vmat->m[2][2] = cos_x2;
        block->vec.vx = cos_y;
        block->vec.vz = sin_y;
        sin_copy      = sin_x;
        neg           = -sin_x;
        vmat->m[1][2] = neg;
        vmat->m[2][1] = sin_copy;
    }

    gte_SetRotMatrix(&block->mat);
    vec = &block->vec;

    gte_ldsv(vec);
    gte_rtir_real();
    {
        u16          sy;
        register u16 cy asm("v1");
        sy = block->sin_y;
        TOUCH_REG(sy);
        cy = cos_y;
        TOUCH_REG_USE(cy, cos_y);
        block->vec.vz = cy;
        block->vec.vx = sy;
    }
    gte_stclmv(&block->mat);

    gte_ldsv(vec);
    gte_rtir_real();
    {
        register u16 cz asm("v0");
        u16          sz;
        cz            = block->cos_z;
        sz            = block->sin_z;
        col2          = (u8*)head - 0x30;
        block->vec.vz = 0;
        block->vec.vx = cz;
        block->vec.vy = sz;
    }
    gte_stclmv(col2);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(vec);
    gte_rtir_real();
    {
        s16 sz;
        u16 cz;
        sz            = block->sin_z;
        cz            = block->cos_z;
        sz            = -sz;
        block->vec.vx = sz;
        block->vec.vy = cz;
    }
    gte_stclmv(&block->mat);

    gte_ldsv(vec);
    gte_rtir_real();
    col1 = (u8*)head - 0x32;
    gte_stclmv(col1);

    if (flag != 0) {
        *(u32*)&out->m[0][0] = *(u32*)(head - 0x34);
        *(u32*)&out->m[0][2] = *(u32*)&block->mat.m[0][2];
        *(u32*)&out->m[1][1] = *(u32*)&block->mat.m[1][1];
        *(u32*)&out->m[2][0] = *(u32*)&block->mat.m[2][0];
        out->m[2][2]         = block->mat.m[2][2];
    } else {
        gte_MulMatrix0_real(out, &block->mat, out);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x34;
    }
}

void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag)
{
    void**                  s;
    u8*                     head;
    register void*          p asm("v0");
    ScratchRotXYZ*          block;
    SVECTOR*                vec;
    volatile ScratchRotXYZ* vblock;
    volatile MATRIX*        vmat;
    u16                     sin_y;
    u16                     cos_x;
    register u16            sin_x asm("a1");
    s16                     neg;
    u16                     cos_y2;
    void*                   col1;
    void*                   col2;

    s     = (void**)G_SCRATCH_HEAD;
    head  = (u8*)*s;
    p     = head - 0x34;
    block = p;
    *s    = p;

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    *(s16*)(head - 0x34) = block->cos_y;
    vblock               = block;
    {
        sin_y         = vblock->sin_y;
        cos_x         = vblock->cos_x;
        sin_x         = vblock->sin_x;
        vmat          = &block->mat;
        vmat->m[0][1] = 0;
        vmat->m[1][0] = 0;
        vmat->m[1][1] = ONE;
        vmat->m[1][2] = 0;
        vmat->m[2][1] = 0;
        block->vec.vx = 0;
        neg           = sin_y;
        TOUCH_REG_USE(neg, sin_y);
        vmat->m[0][2] = sin_y;
        cos_y2        = vblock->cos_y;
        neg           = -neg;
        vmat->m[2][0] = neg;
        block->vec.vy = cos_x;
        block->vec.vz = sin_x;
        vmat->m[2][2] = cos_y2;
    }

    gte_SetRotMatrix(&block->mat);
    vec = &block->vec;

    gte_ldsv(vec);
    gte_rtir_real();
    {
        s16 sx;
        u16 cx;
        sx = sin_x;
        TOUCH_REG_USE(sx, sin_x);
        sx            = -sx;
        cx            = block->cos_x;
        col1          = (u8*)head - 0x32;
        block->vec.vx = 0;
        block->vec.vy = sx;
        block->vec.vz = cx;
    }
    gte_stclmv(col1);

    gte_ldsv(vec);
    gte_rtir_real();
    {
        register u16 cz asm("v0");
        u16          sz;
        cz            = block->cos_z;
        sz            = block->sin_z;
        col2          = (u8*)head - 0x30;
        block->vec.vz = 0;
        block->vec.vx = cz;
        block->vec.vy = sz;
    }
    gte_stclmv(col2);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(vec);
    gte_rtir_real();
    {
        register s16 sz asm("v0");
        u16          cz;
        sz            = block->sin_z;
        cz            = block->cos_z;
        block->vec.vz = 0;
        sz            = -sz;
        block->vec.vx = sz;
        block->vec.vy = cz;
    }
    gte_stclmv(&block->mat);

    gte_ldsv(vec);
    gte_rtir_real();
    gte_stclmv(col1);

    if (flag != 0) {
        *(u32*)&out->m[0][0] = *(u32*)(head - 0x34);
        *(u32*)&out->m[0][2] = *(u32*)&block->mat.m[0][2];
        *(u32*)&out->m[1][1] = *(u32*)&block->mat.m[1][1];
        *(u32*)&out->m[2][0] = *(u32*)&block->mat.m[2][0];
        out->m[2][2]         = block->mat.m[2][2];
    } else {
        gte_MulMatrix0_real(out, &block->mat, out);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x34;
    }
}

void Gfx_RotMatrixZYX(MATRIX* out, SVECTOR* angles, s32 flag)
{
    void**                  s;
    u8*                     head;
    register void*          p asm("v0");
    ScratchRotZYX*          block;
    SVECTOR*                vec;
    volatile ScratchRotZYX* vblock;
    volatile MATRIX*        vmat;
    register u16            sin_z asm("v0");
    u16                     cos_z;
    register u16            cos_y asm("a2");
    s16                     sin_y;
    register s16            neg asm("v0");
    register s16            sin_copy asm("a0");
    void*                   col1;
    void*                   col2;

    s     = (void**)G_SCRATCH_HEAD;
    head  = (u8*)*s;
    p     = head - 0x44;
    block = p;
    *s    = p;

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    *(s16*)(head - 0x44) = block->cos_z;
    vblock               = block;
    vblock->mat.m[2][2]  = ONE;
    {
        sin_z         = vblock->sin_z;
        cos_z         = vblock->cos_z;
        cos_y         = vblock->cos_y;
        sin_y         = vblock->sin_y;
        vmat          = &block->mat;
        vmat->m[0][2] = 0;
        vmat->m[1][2] = 0;
        vmat->m[2][0] = 0;
        vmat->m[2][1] = 0;
        block->vec.vy = 0;
        sin_copy      = sin_z;
        neg           = -sin_z;
        sin_y         = -sin_y;
        vmat->m[0][1] = neg;
        vmat->m[1][0] = sin_copy;
        vmat->m[1][1] = cos_z;
        block->vec.vx = cos_y;
        block->vec.vz = sin_y;
    }

    gte_SetRotMatrix(&block->mat);
    vec = &block->vec;

    gte_ldsv(vec);
    gte_rtir_real();
    {
        u16          sy;
        register u16 cy asm("v1");
        sy = block->sin_y;
        TOUCH_REG(sy);
        cy = cos_y;
        TOUCH_REG_USE(cy, cos_y);
        block->vec3.vy = 0;
        block->vec3.vz = cy;
        block->vec3.vx = sy;
    }
    gte_stclmv(&block->mat);

    gte_ldsv(&block->vec3);
    gte_rtir_real();
    {
        register u16 cx asm("v0");
        u16          sx;
        cx             = block->cos_x;
        sx             = block->sin_x;
        col2           = (u8*)head - 0x40;
        block->vec2.vx = 0;
        block->vec2.vy = cx;
        block->vec2.vz = sx;
    }
    gte_stclmv(col2);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec2);
    gte_rtir_real();
    {
        register s16 sx asm("v0");
        u16          cx;
        sx             = block->sin_x;
        cx             = block->cos_x;
        block->vec3.vx = 0;
        sx             = -sx;
        block->vec3.vy = sx;
        block->vec3.vz = cx;
    }
    col1 = (u8*)head - 0x42;
    gte_stclmv(col1);

    gte_ldsv(&block->vec3);
    gte_rtir_real();
    gte_stclmv(col2);

    if (flag != 0) {
        *(u32*)&out->m[0][0] = *(u32*)(head - 0x44);
        *(u32*)&out->m[0][2] = *(u32*)&block->mat.m[0][2];
        *(u32*)&out->m[1][1] = *(u32*)&block->mat.m[1][1];
        *(u32*)&out->m[2][0] = *(u32*)&block->mat.m[2][0];
        out->m[2][2]         = block->mat.m[2][2];
    } else {
        gte_MulMatrix0_real(out, &block->mat, out);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x44;
    }
}

void Gfx_MatrixToEuler(MATRIX* arg0, SVECTOR* arg1)
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
        u16              sin_u;
        register s16     neg_s asm("v0");
        u16              cos2;
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
        TOUCH_REG_USE(neg_s, sin_u);
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

void Gfx_TransposeRot(MATRIX* arg0, volatile MATRIX* arg1)
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

void Gfx_MatrixCol0(MATRIX* arg0, volatile SVECTOR* arg1)
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

void Gfx_MatrixCol1(MATRIX* arg0, volatile SVECTOR* arg1)
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

void Gfx_MatrixCol2(MATRIX* arg0, volatile SVECTOR* arg1)
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

void Gfx_RotMatrixX(MATRIX* arg0, s32 angle, s32 flag)
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
        u16 cos_u;
        s16 t;

        arg0->m[0][0] = ONE;
        arg0->m[0][1] = 0;
        arg0->m[0][2] = 0;
        arg0->m[1][0] = 0;
        cos_u         = p->cos_val;
        arg0->m[1][1] = cos_u;
        t             = p->sin_val;
        arg0->m[2][0] = 0;
        arg0->m[1][2] = -t;
        arg0->m[2][1] = p->sin_val;
        arg0->m[2][2] = p->cos_val;
    } else {
        register u16         cos_u asm("v0");
        register u16         cos2 asm("a0");
        register u16         sin_u asm("v0");
        s16                  copy;
        volatile MATRIX*     vmat;
        volatile ScratchMat* vblock;

        *(s16*)(head - 0x24) = ONE;
        vmat                 = &block->mat;
        vblock               = block;
        cos_u                = vblock->cos_val;
        cos2                 = vblock->cos_val;
        vmat->m[1][1]        = cos_u;
        sin_u                = block->sin_val;
        vmat->m[0][1]        = 0;
        vmat->m[0][2]        = 0;
        vmat->m[1][0]        = 0;
        vmat->m[2][0]        = 0;
        vmat->m[2][2]        = cos2;
        copy                 = sin_u;
        TOUCH_REG_USE(copy, sin_u);
        vmat->m[1][2] = -sin_u;
        vmat->m[2][1] = copy;

        gte_MulMatrix0_real(arg0, p, arg0);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x24;
    }
}

void Gfx_RotMatrixY(MATRIX* arg0, s32 angle, s32 flag)
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
        u16 sin_u;
        s16 t;

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
        s16              neg_s;
        u16              cos2;
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
        TOUCH_REG_USE(neg_s, sin_u);
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

void Gfx_RotMatrixZ(MATRIX* arg0, s32 angle, s32 flag)
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
        u16 cos_u;
        s16 t;

        arg0->m[0][0] = cos;
        t             = p->sin_val;
        arg0->m[0][2] = 0;
        arg0->m[0][1] = -t;
        arg0->m[1][0] = p->sin_val;
        cos_u         = p->cos_val;
        arg0->m[1][2] = 0;
        arg0->m[2][0] = 0;
        arg0->m[2][1] = 0;
        arg0->m[2][2] = ONE;
        arg0->m[1][1] = cos_u;
    } else {
        register u16     sin_u asm("v0");
        s16              copy;
        u16              cos2;
        volatile MATRIX* vmat;

        *(s16*)(head - 0x24) = cos;
        vmat                 = &block->mat;
        vmat->m[2][2]        = ONE;
        COMPILER_BARRIER();
        sin_u         = block->sin_val;
        cos2          = block->cos_val;
        vmat->m[0][2] = 0;
        vmat->m[1][2] = 0;
        vmat->m[2][0] = 0;
        vmat->m[2][1] = 0;
        copy          = sin_u;
        TOUCH_REG_USE(copy, sin_u);
        vmat->m[0][1] = -sin_u;
        vmat->m[1][0] = copy;
        vmat->m[1][1] = cos2;

        gte_MulMatrix0_real(arg0, p, arg0);
    }

    {
        void** scratch = (void**)G_SCRATCH_HEAD;
        *scratch       = (u8*)*scratch + 0x24;
    }
}

void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out)
{
    register void**            scratch asm("s0");
    u8*                        head;
    ScratchNormBlock*          block;
    register ScratchNormBlock* vec asm("a0");
    s32*                       p_min;
    s32*                       p_tmp;
    s32                        val;
    s32                        shift;
    s32                        t_vy;
    s32                        t_sh;
    s32                        t_vz;
    s32                        t_sh2;

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
        t_vy                   = block->vy;
        USE_REG(t_vy);
        t_sh  = block->lzc_min;
        t_vz  = block->vz;
        t_sh2 = t_sh;
        TOUCH_REG(t_sh2);
        block->vy = t_vy >> t_sh;
        block->vz = t_vz >> t_sh2;
    }

    VectorNormalS((VECTOR*)block, out);

    *scratch = (u8*)*scratch + 0x18;
}

void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2)
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

s32 Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1)
{
    s32 result;

    gte_ldsvrtrow0(arg0);
    gte_ldv0(arg1);
    gte_rtv0sf0();
    gte_stlvnl0(&result);
    return result;
}
