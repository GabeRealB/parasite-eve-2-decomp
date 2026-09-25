#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/gtemac.h>

/// Gfx_NormalizeLightDir's scratch-pad block: the direction being scaled down
/// to fit VectorNormalS, and the leading-zero counts of its components.
typedef struct {
    VECTOR v;
    s32    lzc_min; // fewest leading zeros among the components, then the shift applied
    s32    lzc_tmp; // leading zeros of the component just counted
} ScratchNormBlock;

/// A rotation matrix and the sine and cosine of the angle it was built from,
/// held in the scratchpad arena.
///
/// A routine that needs one takes the block off `G_SCRATCH_HEAD`, builds the
/// rotation into `mat` from the two values, and hands the matrix to the GTE to
/// be applied or to have the angles read back out of it.
typedef struct {
    MATRIX mat;     // the rotation matrix
    s16    sin_val; // sine of the angle it is built from
    s16    cos_val; // cosine of the same angle
} ScratchMat;

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
    ScratchRotXYZ* block;

    block = SCRATCH_PUSH(ScratchRotXYZ);

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0] = ONE;
    block->mat.m[0][1] = 0;
    block->mat.m[0][2] = 0;
    block->mat.m[1][0] = 0;
    block->mat.m[1][1] = block->cos_x;
    block->mat.m[1][2] = -block->sin_x;
    block->mat.m[2][0] = 0;
    block->mat.m[2][1] = block->sin_x;
    block->mat.m[2][2] = block->cos_x;

    block->vec.vx = block->cos_y;
    block->vec.vy = 0;
    block->vec.vz = -block->sin_y;

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = block->sin_y;
    block->vec.vz = block->cos_y;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = block->cos_z;
    block->vec.vy = block->sin_z;
    block->vec.vz = 0;
    gte_stclmv(&block->mat.m[0][2]);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec);
    gte_rtir();
    block->vec.vx = -block->sin_z;
    block->vec.vy = block->cos_z;
    gte_stclmv(&block->mat.m[0][0]);

    gte_ldsv(&block->vec);
    gte_rtir();
    gte_stclmv(&block->mat.m[0][1]);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_POP(ScratchRotXYZ);
}

void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag)
{
    ScratchRotXYZ*          head;
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

    head               = SCRATCH_HEAD(ScratchRotXYZ);
    p                  = head - 1;
    block              = p;
    SCRATCH_HEAD(void) = p;

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0] = block->cos_y;
    vblock             = block;
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
    gte_rtir();
    {
        s16 sx;
        u16 cx;
        sx = sin_x;
        TOUCH_REG_USE(sx, sin_x);
        sx            = -sx;
        cx            = block->cos_x;
        col1          = &block->mat.m[0][1];
        block->vec.vx = 0;
        block->vec.vy = sx;
        block->vec.vz = cx;
    }
    gte_stclmv(col1);

    gte_ldsv(vec);
    gte_rtir();
    {
        register u16 cz asm("v0");
        u16          sz;
        cz            = block->cos_z;
        sz            = block->sin_z;
        col2          = &block->mat.m[0][2];
        block->vec.vz = 0;
        block->vec.vx = cz;
        block->vec.vy = sz;
    }
    gte_stclmv(col2);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(vec);
    gte_rtir();
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
    gte_rtir();
    gte_stclmv(col1);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_POP(ScratchRotXYZ);
}

void Gfx_RotMatrixZYX(MATRIX* out, SVECTOR* angles, s32 flag)
{
    ScratchRotZYX*          head;
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

    head               = SCRATCH_HEAD(ScratchRotZYX);
    p                  = head - 1;
    block              = p;
    SCRATCH_HEAD(void) = p;

    block->sin_x = rsin(angles->vx);
    block->sin_y = rsin(angles->vy);
    block->sin_z = rsin(angles->vz);
    block->cos_x = rcos(angles->vx);
    block->cos_y = rcos(angles->vy);
    block->cos_z = rcos(angles->vz);

    block->mat.m[0][0]  = block->cos_z;
    vblock              = block;
    vblock->mat.m[2][2] = ONE;
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
    gte_rtir();
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
    gte_rtir();
    {
        register u16 cx asm("v0");
        u16          sx;
        cx             = block->cos_x;
        sx             = block->sin_x;
        col2           = &block->mat.m[0][2];
        block->vec2.vx = 0;
        block->vec2.vy = cx;
        block->vec2.vz = sx;
    }
    gte_stclmv(col2);

    gte_SetRotMatrix(&block->mat);
    gte_ldsv(&block->vec2);
    gte_rtir();
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
    col1 = &block->mat.m[0][1];
    gte_stclmv(col1);

    gte_ldsv(&block->vec3);
    gte_rtir();
    gte_stclmv(col2);

    if (flag != 0) {
        MATRIX_PAIR(out, 0, 0) = MATRIX_PAIR(&block->mat, 0, 0);
        MATRIX_PAIR(out, 0, 2) = MATRIX_PAIR(&block->mat, 0, 2);
        MATRIX_PAIR(out, 1, 1) = MATRIX_PAIR(&block->mat, 1, 1);
        MATRIX_PAIR(out, 2, 0) = MATRIX_PAIR(&block->mat, 2, 0);
        out->m[2][2]           = block->mat.m[2][2];
    } else {
        gte_MulMatrix0(out, &block->mat, out);
    }

    SCRATCH_POP(ScratchRotZYX);
}

void Gfx_MatrixToEuler(MATRIX* arg0, SVECTOR* arg1)
{
    u8*         head;
    ScratchMat* block;
    s16         angle;

    head                     = SCRATCH_HEAD(u8);
    block                    = (ScratchMat*)(head - 0x30); // reserves 0x30 for a 0x24-byte block
    SCRATCH_HEAD(ScratchMat) = block;

    angle          = -ratan2(arg0->m[1][2], arg0->m[2][2]);
    arg1->vx       = angle;
    block->sin_val = rsin(angle);
    block->cos_val = rcos(arg1->vx);

    block->mat.m[0][0] = ONE;
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

    gte_MulMatrix0(&block->mat, arg0, &block->mat);

    arg1->vy = ratan2(block->mat.m[0][2], block->mat.m[2][2]);
    arg1->vz = ratan2(block->mat.m[1][0], block->mat.m[1][1]);

    SCRATCH_POP_BYTES(0x30);
}

void Gfx_TransposeRot(MATRIX* arg0, MATRIX* arg1)
{
    gte_TransposeMatrix(arg0, arg1);
}

void Gfx_MatrixCol0(MATRIX* arg0, SVECTOR* arg1)
{
    gte_ReadMatrixColumn(arg0, 0, arg1);
}

void Gfx_MatrixCol1(MATRIX* arg0, SVECTOR* arg1)
{
    gte_ReadMatrixColumn(arg0, 1, arg1);
}

void Gfx_MatrixCol2(MATRIX* arg0, SVECTOR* arg1)
{
    gte_ReadMatrixColumn(arg0, 2, arg1);
}

void Gfx_RotMatrixX(MATRIX* arg0, s32 angle, s32 flag)
{
    ScratchMat* head;
    ScratchMat* block;
    ScratchMat* p;
    s16         cos;

    head                     = SCRATCH_HEAD(ScratchMat);
    block                    = head - 1;
    SCRATCH_HEAD(ScratchMat) = block;
    p                        = block;

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

        block->mat.m[0][0] = ONE;
        vmat               = &block->mat;
        vblock             = block;
        cos_u              = vblock->cos_val;
        cos2               = vblock->cos_val;
        vmat->m[1][1]      = cos_u;
        sin_u              = block->sin_val;
        vmat->m[0][1]      = 0;
        vmat->m[0][2]      = 0;
        vmat->m[1][0]      = 0;
        vmat->m[2][0]      = 0;
        vmat->m[2][2]      = cos2;
        copy               = sin_u;
        TOUCH_REG_USE(copy, sin_u);
        vmat->m[1][2] = -sin_u;
        vmat->m[2][1] = copy;

        gte_MulMatrix0(arg0, p, arg0);
    }

    SCRATCH_POP(ScratchMat);
}

void Gfx_RotMatrixY(MATRIX* arg0, s32 angle, s32 flag)
{
    ScratchMat* head;
    ScratchMat* block;
    ScratchMat* p;
    s16         cos;

    head                     = SCRATCH_HEAD(ScratchMat);
    block                    = head - 1;
    SCRATCH_HEAD(ScratchMat) = block;
    p                        = block;

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

        block->mat.m[0][0] = cos;
        vmat               = &block->mat;
        sin_u              = block->sin_val;
        vmat->m[0][1]      = 0;
        vmat->m[1][0]      = 0;
        vmat->m[1][1]      = ONE;
        vmat->m[1][2]      = 0;
        vmat->m[2][1]      = 0;
        neg_s              = sin_u;
        TOUCH_REG_USE(neg_s, sin_u);
        vmat->m[0][2] = sin_u;
        cos2          = block->cos_val;
        vmat->m[2][0] = -neg_s;
        vmat->m[2][2] = cos2;

        gte_MulMatrix0(arg0, p, arg0);
    }

    SCRATCH_POP(ScratchMat);
}

void Gfx_RotMatrixZ(MATRIX* arg0, s32 angle, s32 flag)
{
    ScratchMat* head;
    ScratchMat* block;
    ScratchMat* p;
    s16         cos;

    head                     = SCRATCH_HEAD(ScratchMat);
    block                    = head - 1;
    SCRATCH_HEAD(ScratchMat) = block;
    p                        = block;

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

        block->mat.m[0][0] = cos;
        vmat               = &block->mat;
        vmat->m[2][2]      = ONE;
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

        gte_MulMatrix0(arg0, p, arg0);
    }

    SCRATCH_POP(ScratchMat);
}

void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out)
{
    register void**            scratch asm("s0");
    ScratchNormBlock*          head;
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

    scratch                                    = SCRATCH_HEAD_ADDR;
    head                                       = SCRATCH_HEAD_AT(scratch, ScratchNormBlock);
    block                                      = head - 1;
    SCRATCH_HEAD_AT(scratch, ScratchNormBlock) = block;

    block->v = *light;

    vec = block;

    gte_ldlzc(vec->v.vx);
    gte_nop();
    gte_nop();
    p_min = &block->lzc_min;
    gte_stlzc(p_min);

    gte_ldlzc(vec->v.vy);
    gte_nop();
    gte_nop();
    p_tmp = &block->lzc_tmp;
    gte_stlzc(p_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    val = vec->v.vz;
    gte_ldlzc(val);
    gte_nop();
    gte_nop();
    gte_stlzc(p_tmp);

    if (block->lzc_min > block->lzc_tmp) {
        block->lzc_min = block->lzc_tmp;
    }

    val = block->lzc_min;
    if (val < 0x12) {
        shift          = 0x12 - val;
        block->lzc_min = shift;
        block->v.vx  >>= shift;
        t_vy           = block->v.vy;
        t_sh           = block->lzc_min;
        t_vz           = block->v.vz;
        t_sh2          = t_sh;
        TOUCH_REG(t_sh2);
        block->v.vy = t_vy >> t_sh;
        block->v.vz = t_vz >> t_sh2;
    }

    VectorNormalS(&block->v, out);

    SCRATCH_POP_AT(scratch, ScratchNormBlock);
}

void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2)
{
    register void**   scratch asm("s1");
    register u8*      head asm("v1");
    register SVECTOR* sv1 asm("a0");
    MATRIX*           mat;
    u16               tmp;

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, u8);

    *(SVECTOR*)(head - 0x1A) = *arg2;

    sv1                              = (SVECTOR*)(head - 0x14);
    tmp                              = arg1->vx;
    sv1->vx                          = tmp;
    mat                              = (MATRIX*)(head - 0x20);
    tmp                              = arg1->vy;
    sv1->vy                          = tmp;
    tmp                              = arg1->vz;
    head                             = head - 0x1A;
    SCRATCH_HEAD_AT(scratch, MATRIX) = mat;
    sv1->vz                          = tmp;

    gte_ldopv1SV(head);
    gte_ldopv2SV(sv1);
    gte_op12();
    gte_stsv(mat);

    MatrixNormal_2(mat, mat);

    gte_TransposeMatrix(mat, out);

    SCRATCH_POP_BYTES_AT(scratch, 0x20);
}

s32 Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1)
{
    s32 result;

    gte_ldsvrtrow0(arg0);
    gte_ldv0(arg1);
    gte_rtv0_sf0();
    gte_stlvnl0(&result);
    return result;
}
