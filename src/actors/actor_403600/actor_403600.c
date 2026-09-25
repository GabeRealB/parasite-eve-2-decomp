#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/rand.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "actors/actors_shared_80131fc8.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor.h"
#include "actors/actor_403600.h"

/// 0x1C-byte scratch block used while building the screen transition grid.
typedef struct Actor403600ScreenScratch {
    /* 0x00 */ u8      pad_0[0x10];
    /* 0x10 */ s32     otz;
    /* 0x14 */ SVECTOR offset;
} Actor403600ScreenScratch;
STATIC_ASSERT_SIZEOF(Actor403600ScreenScratch, 0x1C);

/// 0x78-byte scratch block used to project the radial effect grid.
typedef struct Actor403600EffectScratch {
    /* 0x00 */ s32     dp;
    /* 0x04 */ s32     flag;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     nclip;
    /* 0x10 */ s32     sxy;
    /* 0x14 */ SVECTOR projected;
    /* 0x1C */ SVECTOR vec;
    /* 0x24 */ s32     maxOtz;
    /* 0x28 */ MATRIX  matrix;
    /* 0x48 */ SVECTOR points[3];
    /* 0x60 */ s32     pad_60[2];
    /* 0x68 */ s32     sxy3[3];
    /* 0x74 */ s32     pad_74;
} Actor403600EffectScratch;
STATIC_ASSERT_SIZEOF(Actor403600EffectScratch, 0x78);

extern const SVECTOR D_actor_403600_80131E24;
extern GpU16Pair     D_actor_403600_801420F0;
extern s32           D_actor_403600_80142120[];

void func_actor_403600_80134398(Task* arg0);

#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/* The read/write matrix operand permits the stack-vector address to be
 * scheduled after the matrix load. The vector load consumes that operand to
 * preserve the order of the two GTE transfers. */
#define actor_403600_set_rot_matrix_dep(matrix)                  \
    __asm__("lw $12,0(%0);lw $13,4(%0);ctc2 $12,$0;ctc2 $13,$1;" \
            "lw $12,8(%0);lw $13,12(%0);lw $14,16(%0);"          \
            "ctc2 $12,$2;ctc2 $13,$3;ctc2 $14,$4"                \
            : "+r"(matrix) : : "$12", "$13", "$14", "memory")
#define actor_403600_ldv0_dep(vector, matrix) \
    __asm__ volatile("lwc2 $0,0(%0);lwc2 $1,4(%0)" : : "r"(vector), "r"(matrix))

#define actor_403600_ldsxy3_fifo_gt3(p)                    \
    __asm__ volatile("lw $21, 1(%0); nop; mtc2 $21, $15;"  \
                     "lw $21, 13(%0); nop; mtc2 $21, $15;" \
                     "lw $21, 25(%0); nop; mtc2 $21, $15"  \
                     : : "r"(p) : "$21")
#define actor_403600_ldsxy3_fifo_gt4(p)                    \
    __asm__ volatile("lw $22, 1(%0); nop; mtc2 $22, $15;"  \
                     "lw $22, 13(%0); nop; mtc2 $22, $15;" \
                     "lw $22, 25(%0); nop; mtc2 $22, $15"  \
                     : : "r"(p) : "$22")
#define actor_403600_ldsxy1_fifo_gt4(p) \
    __asm__ volatile("lw $22, 37(%0); nop; mtc2 $22, $15" : : "r"(p) : "$22")
#define actor_403600_ldsz0(v) __asm__ volatile("mtc2 %0, $16" : : "r"(v))
#define actor_403600_ldsz1(v) __asm__ volatile("mtc2 %0, $17" : : "r"(v))
#define actor_403600_ldsz2(v) __asm__ volatile("mtc2 %0, $18" : : "r"(v))
#define actor_403600_ldsz3(v) __asm__ volatile("mtc2 %0, $19" : : "r"(v))
#define actor_403600_fade_rgb(p, scale, first) \
    __asm__ volatile(                          \
        "mult %0, %1\n\t"                      \
        "mflo %0\n\t"                          \
        "lbu $2, -2(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $7\n\t"                          \
        "lbu $2, -1(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $8\n\t"                          \
        "lbu $2, 9(%2)\n\t"                    \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $9\n\t"                          \
        "lbu $2, 10(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $10\n\t"                         \
        "lbu $2, 11(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $11\n\t"                         \
        "lbu $2, 21(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "mflo $12\n\t"                         \
        "lbu $2, 22(%2)\n\t"                   \
        "nop\n\t"                              \
        "mult $2, %1\n\t"                      \
        "sra %0, %0, 7\n\t"                    \
        "sb %0, -3(%2)\n\t"                    \
        "sra %0, $7, 7\n\t"                    \
        "sb %0, -2(%2)\n\t"                    \
        "mflo $13\n\t"                         \
        "lbu $2, 23(%2)\n\t"                   \
        "sra %0, $8, 7\n\t"                    \
        "mult $2, %1\n\t"                      \
        "sb %0, -1(%2)\n\t"                    \
        "sra %0, $9, 7\n\t"                    \
        "sb %0, 9(%2)\n\t"                     \
        "sra %0, $10, 7\n\t"                   \
        "sb %0, 10(%2)\n\t"                    \
        "sra %0, $11, 7\n\t"                   \
        "sb %0, 11(%2)\n\t"                    \
        "sra %0, $12, 7\n\t"                   \
        "sb %0, 21(%2)\n\t"                    \
        "sra %0, $13, 7\n\t"                   \
        "sb %0, 22(%2)\n\t"                    \
        "mflo $7\n\t"                          \
        "sra %0, $7, 7\n\t"                    \
        "sb %0, 23(%2)"                        \
        : "+r"(first)                          \
        : "r"(scale), "r"(p)                   \
        : "$2", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "hi", "lo", "memory")

#define actor_403600_fade_rgb_staged(p, scale, first) \
    __asm__ volatile(                                 \
        "mult %0, %1\n\t"                             \
        "mflo %0\n\t"                                 \
        "lbu $2, -2(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $7\n\t"                                 \
        "lbu $2, -1(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $8\n\t"                                 \
        "lbu $2, 9(%2)\n\t"                           \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $9\n\t"                                 \
        "lbu $2, 10(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $10\n\t"                                \
        "lbu $2, 11(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $11\n\t"                                \
        "lbu $2, 21(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $12\n\t"                                \
        "lbu $2, 22(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "mflo $13\n\t"                                \
        "lbu $2, 23(%2)\n\t"                          \
        "nop\n\t"                                     \
        "mult $2, %1\n\t"                             \
        "sra %0, %0, 7\n\t"                           \
        "sb %0, -3(%2)\n\t"                           \
        "sra %0, $7, 7\n\t"                           \
        "sb %0, -2(%2)\n\t"                           \
        "sra %0, $8, 7\n\t"                           \
        "sb %0, -1(%2)\n\t"                           \
        "sra %0, $9, 7\n\t"                           \
        "mflo $14\n\t"                                \
        "sb %0, 33(%2)\n\t"                           \
        "andi $2, %0, 0xFF\n\t"                       \
        "mult $2, %1\n\t"                             \
        "sra %0, $10, 7\n\t"                          \
        "mflo $8\n\t"                                 \
        "sb %0, 34(%2)\n\t"                           \
        "andi $2, %0, 0xFF\n\t"                       \
        "mult $2, %1\n\t"                             \
        "sra %0, $11, 7\n\t"                          \
        "mflo $7\n\t"                                 \
        "sb %0, 35(%2)\n\t"                           \
        "andi $2, %0, 0xFF\n\t"                       \
        "mult $2, %1\n\t"                             \
        "sra %0, $12, 7\n\t"                          \
        "sb %0, 21(%2)\n\t"                           \
        "sra %0, $13, 7\n\t"                          \
        "sb %0, 22(%2)\n\t"                           \
        "sra %0, $14, 7\n\t"                          \
        "sb %0, 23(%2)\n\t"                           \
        "sra %0, $8, 7\n\t"                           \
        "sb %0, 33(%2)\n\t"                           \
        "sra %0, $7, 7\n\t"                           \
        "sb %0, 34(%2)\n\t"                           \
        "mflo $9\n\t"                                 \
        "sra %0, $9, 7\n\t"                           \
        "sb %0, 35(%2)"                               \
        : "+r"(first)                                 \
        : "r"(scale), "r"(p)                          \
        : "$2", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "hi", "lo", "memory")

extern u8            D_80071090;
extern s32           D_8007107C;
extern TaskDesc      D_actor_403600_801421A0;
extern s32           D_actor_403600_80160698;
extern s32           D_actor_403600_8016069C;
extern s32           D_actor_403600_801606A0;
extern const SVECTOR D_actor_403600_80131E2C;
extern const CVECTOR D_actor_403600_80131E34;

// Typed accesses change GCC 2.8.1's alias/CSE decisions in this initializer.
#define ACTOR_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8*)(expr) + (offset)))

void func_actor_403600_801353D0(ActorEffectState* arg0, GsCOORDINATE2* arg1);
void func_actor_403600_80132A18(Task* arg0, Actor403600Work* arg1, TaskIdMap* arg2, TaskIdMap* arg3);
void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600Work* arg2);

/// Links, at ordering-table depth `otz`, the primitives that copy the frame
/// drawn so far into the 320x240 VRAM rectangle at (0x1C0, 0x100). Linked at
/// one slot they run in reverse: the draw area and offset move to that
/// rectangle, the mask bit is set, a near-black tile clears it and the current
/// draw buffer is copied over it as two 160-pixel 15-bit sprites; then the
/// mask bit is cleared and the draw offset and area return to the draw buffer,
/// clipped to the view's sprite rectangle when its depth lies in front of
/// `otz`. A 0x14-byte block on the scratch stack holds the area and offset for
/// the duration of the call.
void func_actor_403600_801320F8(s32 otz)
{
    u8*                head;
    u8*                allocated;
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;
    s32                val;
    s32                z;

    extra              = Gp_GetViewSprtExtra();
    head               = SCRATCH_HEAD(u8);
    area               = (DR_AREA*)gGpuPrimCursor;
    allocated          = head - 0x14;
    SCRATCH_HEAD(void) = allocated;
    gGpuPrimCursor     = (DR_TPAGE*)(area + 1);
    USE_REG(allocated);
    scratch      = (ActorsDrawScratch*)allocated;
    scratch->otz = otz;
    if (extra != NULL) {
        val = (extra->depth << gDisplayState.otDepthShift) & 0x3FFF;
        z   = otz;
        SOFT_TOUCH_REG(z);
        if ((val >> 4) < z) {
            scratch->rect   = extra->rect;
            scratch->rect.y = (u16)scratch->rect.y + gDisplayState.drawBuffer * 0x110;
        } else {
            goto block_4;
        }
    } else {
    block_4:
        scratch->rect.x = 0;
        scratch->rect.y = D_80070F87[0] * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_POP_BYTES(0x14);
}

void func_actor_403600_801327A0(POLY_FT4* arg0)
{
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_t1;
    s32 temp_t2;
    s32 max;
    s32 min;
    u8  adjust;

    temp_t2 = *(u8*)((s8*)arg0 + 0xC) + *(u8*)((s8*)arg0 + 0x1E);
    min     = temp_t2;
    max     = temp_t2;
    temp_t1 = *(u8*)((s8*)arg0 + 0x14) + *(u8*)((s8*)arg0 + 0x1F);
    temp_a1 = *(u8*)((s8*)arg0 + 0x1C) + *(u8*)((s8*)arg0 + 0x26);
    temp_a0 = *(u8*)((s8*)arg0 + 0x24) + *(u8*)((s8*)arg0 + 0x27);
    if (temp_t1 < min) {
        min = temp_t1;
    } else if (max < temp_t1) {
        max = temp_t1;
    }
    if (temp_a1 < min) {
        min = temp_a1;
    } else if (max < temp_a1) {
        max = temp_a1;
    }
    if (temp_a0 < min) {
        min = temp_a0;
    } else if (max < temp_a0) {
        max = temp_a0;
    }
    if ((max >= 0x100) || (adjust = 0, min >= 0x40)) {
        adjust = 0x40;
    }
    *(s16*)((s8*)arg0 + 0x16) = (s16)(((u32)(adjust + 0x1C0) >> 6) | 0x110);
    *(u8*)((s8*)arg0 + 0xC)   = (u8)(temp_t2 - adjust);
    *(u8*)((s8*)arg0 + 0x14)  = (u8)(temp_t1 - adjust);
    *(u8*)((s8*)arg0 + 0x1C)  = (u8)(temp_a1 - adjust);
    *(u8*)((s8*)arg0 + 0x24)  = (u8)(temp_a0 - adjust);
    *(u8*)((s8*)arg0 + 0x27)  = adjust;
    *(u8*)((s8*)arg0 + 0x26)  = adjust;
    *(u8*)((s8*)arg0 + 0x1F)  = adjust;
    *(u8*)((s8*)arg0 + 0x1E)  = adjust;
}

void func_actor_403600_8013289C(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s16 temp_t3;
    s16 temp_t5;
    s16 var_v0;
    s16 var_v0_3;
    s32 temp_a1;
    s32 temp_v0;
    s32 temp_v1;
    s32 temp_v1_2;
    s32 var_v0_2;
    s32 var_v0_4;
    s32 var_t0;
    u8  temp_v0_2;
    u8* var_t2;

    switch (arg1) {
        case 0:
            arg2   = arg0 + 8;
            var_t2 = (u8*)(arg0 + 0x1E);
            break;
        case 1:
            arg2   = arg0 + 0x10;
            var_t2 = (u8*)(arg0 + 0x1F);
            break;
        case 2:
            arg2   = arg0 + 0x18;
            var_t2 = (u8*)(arg0 + 0x26);
            break;
        default:
            arg2   = arg0 + 0x20;
            var_t2 = (u8*)(arg0 + 0x27);
            break;
    }
    SOFT_TOUCH_REG(arg2);
    temp_t3                 = *(s16*)arg2;
    temp_t5                 = *(s16*)(arg2 + 2);
    arg0                    = temp_t3 + 0xA0;
    var_t0                  = temp_t5 + 0x78;
    temp_a1                 = (D_actor_403600_80160698 * 5) + 0x71357911;
    D_actor_403600_80160698 = temp_a1;
    if (((temp_a1 >> 0x10) & 0xFFF) < (arg3 + 0x400)) {
        temp_v1                 = (temp_a1 * 5) + 0x71357911;
        arg0                    = temp_t3 + 0x9C;
        arg0                   += (temp_v1 >> 0x10) & 7;
        temp_v0                 = (temp_v1 * 5) + 0x71357911;
        D_actor_403600_80160698 = temp_v0;
        temp_v1_2               = temp_t5 + 0x74;
        var_t0                  = temp_v1_2 + ((temp_v0 >> 0x10) & 7);
    }
    if (var_t0 >= 0xF0) {
        var_v0 = ((u16) * (s16*)(arg2 + 2) + 0xEF) - var_t0;
        var_t0 = 0xEF;
        goto block_15;
    }
    var_v0_2 = arg0 < 0x140;
    if (var_t0 < 0) {
        var_v0 = (u16) * (s16*)(arg2 + 2) - var_t0;
        var_t0 = 0;
    block_15:
        *(s16*)(arg2 + 2) = var_v0;
        var_v0_2          = arg0 < 0x140;
    }
    if (var_v0_2 == 0) {
        var_v0_3 = ((u16) * (s16*)arg2 + 0x13F) - arg0;
        arg0     = 0x13F;
        goto block_20;
    }
    var_v0_4 = arg0 < 0x100;
    if (arg0 < 0) {
        var_v0_3 = (u16) * (s16*)arg2 - arg0;
        arg0     = 0;
    block_20:
        *(s16*)arg2 = var_v0_3;
        var_v0_4    = arg0 < 0x100;
    }
    *var_t2 = 0;
    if (var_v0_4 == 0) {
        *var_t2 = 0x40;
    }
    temp_v0_2        = *var_t2;
    *(s8*)(arg2 + 5) = var_t0;
    *(s8*)(arg2 + 4) = (s8)(arg0 - temp_v0_2);
}

void func_actor_403600_80132A18(Task* arg0, Actor403600Work* arg1, TaskIdMap* arg2, TaskIdMap* arg3)
{
    s32                       fade;
    s32                       x;
    s32                       y;
    s32*                      ot_entry;
    s32                       fade_step;
    s32                       green;
    s32                       red;
    s32                       seed;
    s32                       fade_delta;
    s32                       red_sum;
    u32                       sign;
    TILE*                     tile;
    DR_TPAGE*                 draw_mode;
    POLY_FT4*                 poly;
    Actor403600ScreenScratch* scratch;
    s8*                       head;
    POLY_FT4*                 previous;
    POLY_FT4*                 previous_row;
    POLY_FT4*                 previous_top;
    u_long*                   mode_ot;
    u32                       mask;
    u32                       mask_hi;
    u32                       tpage;
    u32                       color;
    s32*                      ot;
    u32                       high, low, high2, low2;

    head               = SCRATCH_HEAD(void) - 0x1C;
    SCRATCH_HEAD(void) = head;
    scratch            = (Actor403600ScreenScratch*)head;
    if (Gp_StateF0.field_4 == 0) {
        seed                    = rand();
        D_actor_403600_80160698 = seed;
        *(s32*)((s8*)arg2 + 4)  = seed;
    } else {
        D_actor_403600_80160698 = *(s32*)((s8*)arg2 + 4);
    }
    scratch->offset.vx = 0;
    scratch->offset.vy = 0;
    scratch->offset.vz = 0;
    fade               = arg1->field_708;
    SCHED_BARRIER();
    y          = -0x78;
    fade_delta = fade - 0xC00;
    fade_step  = fade_delta >> 3;
    sign       = (u32)fade_delta >> 0x1F;
    SCHED_BARRIER();
    x = -0xA0;
    do {
        green = ((((s32)(fade_step + sign) >> 1) + 0x80) & 0xFF) << 8;
        __asm__("addiu %0, %1, 0x7F"
                : "=r"(red_sum)
                : "r"(fade_step), "r"(green));
        __asm__("andi %0, %1, 0xFF" : "=r"(red) : "r"(red_sum));
        SOFT_USE_REG(red);
        SOFT_USE_REG(red);
    next_quad:
        poly                    = (POLY_FT4*)D_actor_403600_8016069C;
        D_actor_403600_8016069C = (s32)(poly + 1);
        previous                = poly - 1;
        if (x == -0xA0) {
            poly->x2 = x;
            poly->y2 = (s16)(y + 0x10);
            func_actor_403600_8013289C((s32)poly, 2, (s32)&scratch->offset, fade);
        } else {
            *(s32*)&poly->x2      = *(s32*)&previous->x3;
            poly->u2              = previous->u3;
            poly->v2              = previous->v3;
            ((u8*)&poly->pad2)[0] = ((u8*)&previous->pad2)[1];
        }
        if (y == -0x78) {
            previous_top = poly - 1;
            if (x == -0xA0) {
                poly->x0 = x;
                poly->y0 = y;
                func_actor_403600_8013289C((s32)poly, 0, (s32)&scratch->offset, fade);
            } else {
                *(s32*)&poly->x0      = *(s32*)&previous_top->x1;
                poly->u0              = previous_top->u1;
                poly->v0              = previous_top->v1;
                ((u8*)&poly->pad1)[0] = ((u8*)&previous_top->pad1)[1];
            }
            poly->x1 = (s16)(x + 0x10);
            poly->y1 = y;
            func_actor_403600_8013289C((s32)poly, 1, (s32)&scratch->offset, fade);
        } else {
            previous_row          = poly - 20;
            *(s32*)&poly->x0      = *(s32*)&previous_row->x2;
            poly->u0              = previous_row->u2;
            poly->v0              = previous_row->v2;
            ((u8*)&poly->pad1)[0] = ((u8*)&previous_row->pad2)[0];
            *(s32*)&poly->x1      = *(s32*)&previous_row->x3;
            poly->u1              = previous_row->u3;
            poly->v1              = previous_row->v3;
            ((u8*)&poly->pad1)[1] = ((u8*)&previous_row->pad2)[1];
        }
        poly->x3 = (s16)(x + 0x10);
        poly->y3 = (s16)(y + 0x10);
        func_actor_403600_8013289C((s32)poly, 3, (s32)&scratch->offset, fade);
        func_actor_403600_801327A0(poly);
        if (fade < 0xC00) {
            ((u8*)&poly->tag)[3] = 9;
            poly->code           = 0x2D;
        } else {
            *(s32*)&poly->r0     = (green | 0x800000) | red;
            ((u8*)&poly->tag)[3] = 9;
            poly->code           = 0x2C;
        }
        __asm__ volatile("lui %0, 0xFF; ori %0, %0, 0xFFFF" : "=r"(mask));
        ot = (s32*)gGpuCurrentOt;
        __asm__ volatile("lui %0, 0xFF00" : "=r"(mask_hi));
        scratch->otz = 0;
        high         = poly->tag & mask_hi;
        low          = *ot & mask;
        SOFT_USE_REG2(high, mask);
        poly->tag = high | low;
        x        += 0x10;
        ot_entry  = (s32*)((*(volatile s32*)&scratch->otz * 4) + (s32)ot);
        high2     = *ot_entry & mask_hi;
        low2      = (s32)poly & mask;
        USE_REG3(high2, high2, low2);
        *ot_entry = high2 | low2;
        if (x < 0xA0) {
            goto next_quad;
        }
        y += 0x10;
        x  = -0xA0;
    } while (y < 0x78);
    TOUCH_REG(x);
    if (fade == 0x1000) {
        color = 0x2060C0;
        tpage = 0xE1000000;
        TOUCH_REG_USE(tpage, color);
        tile                                  = (TILE*)D_actor_403600_8016069C;
        D_actor_403600_8016069C               = (s32)(tile + 1);
        tile->x0                              = -0xA0;
        tile->y0                              = -0x78;
        tile->w                               = 0x140;
        tile->h                               = 0xF0;
        ((u8*)&tile->tag)[3]                  = 3;
        *(s32*)&tile->r0                      = color;
        tile->code                            = 0x62;
        draw_mode                             = gGpuPrimCursor;
        gGpuPrimCursor                        = draw_mode + 1;
        tile->tag                             = (tile->tag & mask_hi) | (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask);
        ((POLY_FT4*)(gGpuCurrentOt - 1))->tag = (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask_hi) | ((u32)tile & mask);
        ((u8*)&draw_mode->tag)[3]             = 1;
        mode_ot                               = gGpuCurrentOt - 1;
        draw_mode->code[0]                    = tpage | 0x220;
        draw_mode->tag                        = (draw_mode->tag & mask_hi) | (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask);
        ((POLY_FT4*)mode_ot)->tag             = (((POLY_FT4*)mode_ot)->tag & mask_hi) | ((u32)draw_mode & mask);
    }
    func_actor_403600_801320F8(0);
    SCRATCH_POP_BYTES(0x1C);
}

void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600Work* arg2)
{
    SVECTOR    local;
    SVECTOR*   local4;
    SVECTOR*   local0;
    Task*      actor;
    u8*        coords;
    u8*        center;
    u8*        head;
    u8*        scratch;
    u8*        coord0;
    u8*        coord3;
    u8*        coord4;
    u8*        current0;
    u8*        current1;
    u8*        current2;
    u8*        current4;
    u8*        saved4;
    u8*        saved3;
    u8*        saved2;
    u8*        column1;
    u8*        column2;
    u8*        firstOutput;
    u8*        stepOutput;
    u8*        secondOutput;
    u8*        thirdOutput;
    u8*        next2;
    SVECTOR*   output0;
    MATRIX*    centerBasis3;
    SVECTOR*   output4;
    MATRIX*    matrix3;
    MATRIX*    matrix4;
    MATRIX*    basis3;
    MATRIX*    basis4;
    MATRIX*    basis0;
    MATRIX*    worldArg0;
    MATRIX*    worldArg1;
    MATRIX*    transposed0;
    MATRIX*    transposed1;
    u8*        viewWorld0;
    u8*        viewWorld1;
    u8*        viewCoord0;
    u8*        viewCoord4;
    s32        i;
    s32        j;
    s32        offset0;
    s32        offset1;
    s32        offset2;
    s16        value;
    u16        x4;
    u16        y4;
    u16        z4;
    u16        stepHeight;
    TmdObject* node4;
    u16        neg0;
    u16        neg1;
    u16        neg2;
    u16        old18;
    u16        oldc;

    actor  = arg0->parent;
    coords = (u8*)((TmdObject*)actor->extra)->coords;
    center = coords + 0x280;
    if (Gp_StateF0.field_4 == 0) {
        head    = SCRATCH_HEAD(u8);
        scratch = (SCRATCH_HEAD(u8) = head - 0x88);
        Gp_UpdateCoord((GsCOORDINATE2*)((u8*)((TmdObject*)actor->extra)->coords + 0x370));
        if (*(s32*)((u8*)arg2 + 0x118) == 0) {
            viewWorld0 = (u8*)&Gfx_ViewWorldMtx;
            worldArg0  = (MATRIX*)viewWorld0;
            TOUCH_REG(worldArg0);
            transposed0 = (MATRIX*)(scratch + 0x10);
            TransposeMatrix(worldArg0, transposed0);
            viewWorld0           -= 0x24;
            *(s16*)(scratch + 8)  = *(u16*)(center + 0x38) - *(u16*)(viewWorld0 + 0x38);
            *(s16*)(scratch + 10) = *(u16*)(center + 0x3C) - *(u16*)(viewWorld0 + 0x3C);
            *(s16*)(scratch + 12) = *(u16*)(center + 0x40) - *(u16*)(viewWorld0 + 0x40);

            SCHED_BARRIER();
            firstOutput = scratch + 8;
            TOUCH_REG(firstOutput);
            local = *(SVECTOR*)(scratch + 8);
            actor_403600_set_rot_matrix_dep(transposed0);
            actor_403600_ldv0_dep(&local, transposed0);
            gte_rtv0();
            gte_stsv(firstOutput);

            *(s16*)(scratch + 0) = 0;
            *(s16*)(scratch + 2) = 0;
            *(s16*)(scratch + 4) = -0x485;
            local                = *(SVECTOR*)scratch;
            gte_SetRotMatrix(center + 0x24);
            gte_ldv0(&local);
            gte_rtv0();
            gte_stsv(scratch);

            local = *(SVECTOR*)scratch;
            gte_SetRotMatrix(transposed0);
            gte_ldv0(&local);
            gte_rtv0();
            gte_stsv(scratch);

            current0 = (u8*)arg2;
            i        = 0;
            do {
                *(SVECTOR*)(current0 + 8) = *(SVECTOR*)(scratch + 8);
                *(s16*)(current0 + 8)    += *(s16*)(scratch + 0) * i;
                *(s16*)(current0 + 10)   += *(s16*)(scratch + 2) * i;
                *(s16*)(current0 + 12)   += *(s16*)(scratch + 4) * i;
                current0                 += 8;
                i++;
            } while (i < 4);

            i          = 0;
            viewCoord0 = (u8*)&gGfxViewCoord;
            basis0     = (MATRIX*)(scratch + 0x10);
            output0    = (SVECTOR*)(scratch + 8);
            local0     = &local;
            offset0    = 0x4B0;
            do {
                current1 = (u8*)arg2 + i * 8;
                coord0   = (u8*)((TmdObject*)actor->extra)->coords + offset0;
                Gp_UpdateCoord((GsCOORDINATE2*)coord0);
                *(s16*)(scratch + 8)  = *(u16*)(coord0 + 0x38) - *(u16*)(viewCoord0 + 0x38);
                *(s16*)(scratch + 10) = *(u16*)(coord0 + 0x3C) - *(u16*)(viewCoord0 + 0x3C);
                *(s16*)(scratch + 12) = *(u16*)(coord0 + 0x40) - *(u16*)(viewCoord0 + 0x40);
                local                 = *(SVECTOR*)(scratch + 8);
                gte_SetRotMatrix(basis0);
                gte_ldv0(local0);
                gte_rtv0();
                gte_stsv(output0);

                *(s16*)(scratch + 0) = 0;
                *(s16*)(scratch + 2) = 0x898;
                *(s16*)(scratch + 4) = 0;
                local                = *(SVECTOR*)scratch;
                gte_SetRotMatrix(center + 0x24);
                gte_ldv0(local0);
                gte_rtv0();
                gte_stsv(scratch);

                local = *(SVECTOR*)scratch;
                gte_SetRotMatrix(basis0);
                gte_ldv0(local0);
                gte_rtv0();
                gte_stsv(scratch);
                *(s16*)(current1 + 0x108) = *(u16*)(scratch + 8) + *(u16*)(scratch + 0);
                *(s16*)(current1 + 0x10A) = *(u16*)(scratch + 10) + *(u16*)(scratch + 2);
                *(s16*)(current1 + 0x10C) = *(u16*)(scratch + 12) + *(u16*)(scratch + 4);
                offset0                  += 0x140;
                i++;
            } while (i < 2);
            *(s32*)((u8*)arg2 + 0x118) += 1;
        } else {
            viewWorld1 = (u8*)&Gfx_ViewWorldMtx;
            worldArg1  = (MATRIX*)viewWorld1;
            TOUCH_REG(worldArg1);
            transposed1 = (MATRIX*)(scratch + 0x10);
            TransposeMatrix(worldArg1, transposed1);
            viewWorld1           -= 0x24;
            *(s16*)(scratch + 8)  = *(u16*)(center + 0x38) - *(u16*)(viewWorld1 + 0x38);
            *(s16*)(scratch + 10) = *(u16*)(center + 0x3C) - *(u16*)(viewWorld1 + 0x3C);
            *(s16*)(scratch + 12) = *(u16*)(center + 0x40) - *(u16*)(viewWorld1 + 0x40);
            stepOutput            = scratch + 8;
            TOUCH_REG(stepOutput);
            local = *(SVECTOR*)(scratch + 8);
            actor_403600_set_rot_matrix_dep(transposed1);
            actor_403600_ldv0_dep(&local, transposed1);
            gte_rtv0();
            gte_stsv(stepOutput);
            *(SVECTOR*)((u8*)arg2 + 8) = *(SVECTOR*)(scratch + 8);

            *(s16*)(scratch + 8)  = 0;
            *(s16*)(scratch + 10) = 0;
            stepHeight            = *(u16*)((u8*)arg1 + 0x70C);
            SOFT_TOUCH_REG_USE(stepOutput, stepHeight);
            *(s16*)(scratch + 12) = -(stepHeight + 0x200);
            secondOutput          = stepOutput;
            SOFT_TOUCH_REG(secondOutput);
            local = *(SVECTOR*)(scratch + 8);
            gte_SetRotMatrix(center + 0x24);
            gte_ldv0(&local);
            gte_rtv0();
            gte_stsv(secondOutput);
            thirdOutput = stepOutput;
            SOFT_TOUCH_REG(thirdOutput);
            local = *(SVECTOR*)(scratch + 8);
            gte_SetRotMatrix(transposed1);
            gte_ldv0(&local);
            gte_rtv0();
            gte_stsv(thirdOutput);

            if (*(s16*)((u8*)arg1 + 0x70A) != 0) {
                gte_lddp(*(u16*)((u8*)arg1 + 0x70A));
                gte_ldsv(stepOutput);
                gte_gpf12();
                gte_stsv(scratch + 0x80);
            }

            i = 0;
            do {
                j = i + 1;
                TOUCH_REG(j);
                next2                   = (u8*)arg2 + j * 8;
                current2                = (u8*)arg2 + i * 8;
                *(s16*)(scratch + 0)    = *(s16*)(next2 + 8) - *(s16*)(current2 + 8);
                *(s16*)(scratch + 2)    = *(s16*)(next2 + 10) - *(s16*)(current2 + 10);
                *(s16*)(scratch + 4)    = *(s16*)(next2 + 12) - *(s16*)(current2 + 12);
                *(s16*)(scratch + 0)   += *(s16*)(scratch + 8);
                *(s16*)(scratch + 2)   += *(s16*)(scratch + 10);
                *(s16*)(scratch + 4)   += *(s16*)(scratch + 12);
                *(s16*)(scratch + 8)  >>= 1;
                *(s16*)(scratch + 10) >>= 1;
                *(s16*)(scratch + 12) >>= 1;
                VectorNormalSS((SVECTOR*)scratch, (SVECTOR*)scratch);
                saved2                     = scratch + i * 8;
                *(SVECTOR*)(saved2 + 0x60) = *(SVECTOR*)scratch;
                gte_lddp(0x485);
                gte_ldsv(scratch);
                gte_gpf12();
                gte_stsv(scratch);
                *(s16*)(next2 + 8)  = *(u16*)(current2 + 8) + *(u16*)(scratch + 0);
                *(s16*)(next2 + 10) = *(u16*)(current2 + 10) + *(u16*)(scratch + 2);
                *(s16*)(next2 + 12) = *(u16*)(current2 + 12) + *(u16*)(scratch + 4);
                i                   = j;
            } while (i < 3);

            i            = 0;
            centerBasis3 = (MATRIX*)(center + 0x24);
            matrix3      = (MATRIX*)(scratch + 0x30);
            basis3       = (MATRIX*)(scratch + 0x10);
            column1      = scratch + 0x32;
            column2      = scratch + 0x34;
            SCHED_BARRIER();
            offset1 = 0x2D0;
            SOFT_USE_REG(center);
            do {
                saved3 = (u8*)(i * 8 + (u32)scratch);
                SOFT_TOUCH_REG_USE(saved3, actor);
                saved3 += 0x60;
                coord3  = (u8*)((TmdObject*)actor->extra)->coords + offset1;
                gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                gte_ldv0(saved3);
                gte_rtv0();
                gte_stsv(scratch);
                TransposeMatrix(centerBasis3, matrix3);
                local = *(SVECTOR*)scratch;
                gte_SetRotMatrix(matrix3);
                gte_ldv0(&local);
                gte_rtv0();
                gte_stsv(scratch);
                *(s16*)(scratch + 8)  = 0;
                *(s16*)(scratch + 10) = 0x1000;
                *(s16*)(scratch + 12) = 0;
                *(s16*)(scratch + 0)  = -*(s16*)(scratch + 0);
                *(s16*)(scratch + 2)  = -*(s16*)(scratch + 2);
                *(s16*)(scratch + 4)  = -*(s16*)(scratch + 4);
                Gfx_OrthonormalBasis(basis3, (SVECTOR*)scratch, (SVECTOR*)(scratch + 8));

                gte_SetRotMatrix(matrix3);
                gte_ldclmv(coord3 + 0x24);
                gte_rtir();
                gte_stclmv(matrix3);
                gte_ldclmv(coord3 + 0x26);
                gte_rtir();
                gte_stclmv(column1);
                gte_ldclmv(coord3 + 0x28);
                gte_rtir();
                gte_stclmv(column2);

                gte_SetRotMatrix(basis3);
                gte_ldclmv(matrix3);
                gte_rtir();
                gte_stclmv(matrix3);
                gte_ldclmv(column1);
                gte_rtir();
                gte_stclmv(column1);
                gte_ldclmv(column2);
                gte_rtir();
                gte_stclmv(column2);

                gte_SetRotMatrix(centerBasis3);
                gte_ldclmv(matrix3);
                gte_rtir();
                gte_stclmv(matrix3);
                gte_ldclmv(column1);
                gte_rtir();
                gte_stclmv(column1);
                gte_ldclmv(column2);
                gte_rtir();
                gte_stclmv(column2);

                TransposeMatrix((MATRIX*)(*(u8**)(coord3 + 0x4C) + 0x24), basis3);
                gte_SetRotMatrix(basis3);
                gte_ldclmv(matrix3);
                gte_rtir();
                gte_stclmv(coord3 + 4);
                gte_ldclmv(column1);
                gte_rtir();
                gte_stclmv(coord3 + 6);
                gte_ldclmv(column2);
                gte_rtir();
                gte_stclmv(coord3 + 8);
                *(s32*)coord3 = 0;
                Gp_UpdateCoord((GsCOORDINATE2*)coord3);
                offset1 += 0x50;
                i++;
            } while (i < 3);

            i        = 0;
            basis4   = (MATRIX*)(scratch + 0x10);
            output4  = (SVECTOR*)(scratch + 8);
            local4   = &local;
            saved4   = scratch;
            current4 = (u8*)arg2;
            offset2  = 0x4B0;
            do {
                SOFT_TOUCH_REG(current4);
                node4 = actor->extra;
                SOFT_USE_REG(node4);
                coord4 = (u8*)node4->coords + offset2;
                TransposeMatrix(&Gfx_ViewWorldMtx, basis4);
                Gp_UpdateCoord((GsCOORDINATE2*)coord4);
                viewCoord4            = (u8*)&gGfxViewCoord;
                *(s16*)(scratch + 8)  = *(u16*)(coord4 + 0x38) - *(u16*)(viewCoord4 + 0x38);
                *(s16*)(scratch + 10) = *(u16*)(coord4 + 0x3C) - *(u16*)(viewCoord4 + 0x3C);
                *(s16*)(scratch + 12) = *(u16*)(coord4 + 0x40) - *(u16*)(viewCoord4 + 0x40);
                local                 = *(SVECTOR*)(scratch + 8);
                gte_SetRotMatrix(basis4);
                gte_ldv0(local4);
                gte_rtv0();
                gte_stsv(output4);

                *(s16*)(scratch + 0) = 0;
                *(s16*)(scratch + 2) = *(s16*)((u8*)arg1 + 0x70E) + 0x200;
                *(s16*)(scratch + 4) = 0;
                local                = *(SVECTOR*)scratch;
                gte_SetRotMatrix(coord4 + 0x24);
                gte_ldv0(local4);
                gte_rtv0();
                gte_stsv(scratch);
                local = *(SVECTOR*)scratch;
                gte_SetRotMatrix(basis4);
                gte_ldv0(local4);
                gte_rtv0();
                gte_stsv(scratch);

                *(s16*)(scratch + 0) += *(s16*)(current4 + 0x108) - *(s16*)(scratch + 8);
                *(s16*)(scratch + 2) += *(s16*)(current4 + 0x10A) - *(s16*)(scratch + 10);
                *(s16*)(scratch + 4) += *(s16*)(current4 + 0x10C) - *(s16*)(scratch + 12);
                if (*(s16*)((u8*)arg1 + 0x70A) != 0) {
                    *(s16*)(scratch + 0) += *(s16*)(scratch + 0x80);
                    *(s16*)(scratch + 2) += *(s16*)(scratch + 0x82);
                    *(s16*)(scratch + 4) += *(s16*)(scratch + 0x84);
                }
                VectorNormalSS((SVECTOR*)scratch, (SVECTOR*)scratch);
                *(SVECTOR*)(saved4 + 0x60) = *(SVECTOR*)scratch;
                gte_lddp(0x898);
                gte_ldsv(scratch);
                gte_gpf12();
                gte_stsv(scratch);
                *(s16*)(current4 + 0x108) = *(u16*)(scratch + 8) + *(u16*)(scratch + 0);
                *(s16*)(current4 + 0x10A) = *(u16*)(scratch + 10) + *(u16*)(scratch + 2);
                *(s16*)(current4 + 0x10C) = *(u16*)(scratch + 12) + *(u16*)(scratch + 4);

                gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                gte_ldv0(saved4 + 0x60);
                gte_rtv0();
                gte_stsv(scratch);
                matrix4 = (MATRIX*)(scratch + 0x30);
                TransposeMatrix((MATRIX*)(coord4 + 0x24), matrix4);
                local = *(SVECTOR*)scratch;
                gte_SetRotMatrix(matrix4);
                gte_ldv0(local4);
                gte_rtv0();
                gte_stsv(scratch);
                *(s16*)(scratch + 8)  = 0;
                *(s16*)(scratch + 10) = 0;
                *(s16*)(scratch + 12) = 0x1000;
                Gfx_OrthonormalBasis(basis4, (SVECTOR*)scratch, output4);
                ACTOR_COPY_MATRIX_COLUMN_TO_SV(basis4, output4, 4, 10, 16);

                neg0  = *(u16*)(scratch + 0x10);
                neg1  = *(u16*)(scratch + 0x16);
                old18 = *(u16*)(scratch + 0x18);
                x4    = *(u16*)(scratch + 0x1E);
                y4    = *(u16*)(scratch + 8);
                z4    = *(u16*)(scratch + 10);
                oldc  = *(u16*)(scratch + 12);
                SCHED_BARRIER();
                *(s16*)(scratch + 0x10) = -neg0;
                neg2                    = *(u16*)(scratch + 0x1C);
                *(s16*)(scratch + 0x16) = -neg1;
                SCHED_BARRIER();
                value                   = *(s16*)(scratch + 0x12);
                *(s16*)(scratch + 0x1A) = old18;
                *(s16*)(scratch + 0x20) = x4;
                *(s16*)(scratch + 0x12) = y4;
                *(s16*)(scratch + 0x18) = z4;
                *(s16*)(scratch + 0x1E) = oldc;
                *(s16*)(scratch + 0x1C) = -neg2;
                *(s16*)(scratch + 0x14) = value;

                gte_SetRotMatrix(basis4);
                gte_ldclmv(coord4 + 4);
                gte_rtir();
                gte_stclmv(coord4 + 4);
                gte_ldclmv(coord4 + 6);
                gte_rtir();
                gte_stclmv(coord4 + 6);
                gte_ldclmv(coord4 + 8);
                gte_rtir();
                gte_stclmv(coord4 + 8);
                *(s32*)coord4 = 0;
                Gp_UpdateCoord((GsCOORDINATE2*)coord4);
                saved4   += 8;
                current4 += 8;
                offset2  += 0x140;
                i++;
            } while (i < 2);
        }
        SCRATCH_POP_BYTES(0x88);
    }
}

void func_actor_403600_80134288(Task* arg0)
{
    Task*                     temp_v0_2;
    TaskIdMap*                temp_a3;
    register TaskIdMap*       temp_v0 asm("a3");
    register Actor403600Work* var_a2 asm("a2");

    var_a2 = (Actor403600Work*)arg0->parent->work;
    if (arg0->state == 0) {
        temp_v0 = memCalloc(0x11C, false);
        if (temp_v0 == NULL) {
            Task_CallExit(arg0);
            return;
        }
        gGameSession->field_80 = 0;
        arg0->work             = temp_v0;
        temp_v0_2              = Task_SpawnFromTable(&D_actor_403600_801421A0, 2, 0, 0);
        if (temp_v0_2 != NULL) {
            Task_Reparent(arg0, temp_v0_2);
        }
        var_a2                  = (Actor403600Work*)arg0->parent->work;
        var_a2->field_710       = arg0;
        D_actor_403600_801606A0 = 0;
        arg0->state            += 1;
        goto block_6;
    }
block_6:
    temp_a3                 = arg0->work;
    D_actor_403600_8016069C = (s32)D_8005C374 + (D_8007107C * 0xC000);
    {
        register s32 field_742 asm("v1");

        field_742 = var_a2->field_742;
        if ((field_742 != 1) && (var_a2->field_708 > 0)) {
            func_actor_403600_80132A18(arg0, var_a2, temp_a3, temp_a3);
        }
    }
}
const SVECTOR D_actor_403600_80131E24 = { -100, 700, -280, 0 };

void func_actor_403600_80134398(Task* arg0)
{
    void*    gteValue1;
    void*    gteValue2;
    void*    gteValue3;
    void*    gteValue4;
    void*    gteValue5;
    void*    gteValue6;
    s32      gteValue7;
    SVECTOR  sp10;
    SVECTOR  sp18;
    SVECTOR* firstVector;
    SVECTOR* cameraVector;
    void*    sp20;
    s32      sp24;
    s32      sp28;
    s32*     sp30;
    s32*     ot;
    u32      otOffset;
    s32      sp34;
    s32*     var_a1_3;
    s16      temp_a2;
    s16      temp_s0_6;
    s16      temp_v1_10;
    s16      temp_v1_11;
    s16      temp_v1_8;
    s16      temp_v1_9;
    s16      var_v0_2;
    s32*     temp_v0_6;
    s32      temp_a0_4;
    s32      temp_v1_13;
    s32      temp_v1_6;
    s32      var_a0;
    s32      var_a1;
    s32      var_a1_2;
    s32      var_a2;
    s32      var_fp;
    s32      var_s4;
    s32      var_v1_2;
    s32      temp_s0_3;
    s32      temp_s0_5;
    s8       temp_v1_12;
    s8       temp_v1_14;
    u16      temp_v1_3;
    s32      var_v0;
    s32      flag;
    s32      historyDst, historySrc;
    SVECTOR* historyOut;
    s32      weightedX, weightedY, weightedZ;
    s32      scratchX, scratchY, scratchZ;
    s32      velocityX, velocityY, velocityZ;
    u8       temp_v0_5;
    u8       temp_v1_4;
    u8       temp_v1_5;
    u8       temp_v1_7;
    void*    temp_a0;
    void*    temp_a0_2;
    void*    temp_a0_3;
    void*    motionParent;
    void*    temp_s0;
    void*    temp_s0_2;
    void*    temp_s0_4;
    void*    temp_s1;
    void*    temp_s1_2;
    void*    temp_s1_3;
    void*    temp_s1_4;
    s32      temp_s2;
    void*    temp_s4;
    void*    temp_s5;
    void*    temp_s6;
    /* Keep the raw scratch head separate until the branch-delay copy. */
    register void* temp_v0 asm("v0");
    void*          temp_v0_2;
    void*          temp_v0_3;
    void*          temp_v0_4;
    void*          temp_v1;
    void*          temp_v1_2;
    void*          var_s1;
    void*          var_v1;
    void*          var_v1_3;

    temp_s6 = ACTOR_FIELD(ACTOR_FIELD(arg0, s32*, 0x2C), s32*, 0x8);
    var_fp  = (s32)&sp10;
    sp10    = D_actor_403600_80131E24;
    /* Order the vector copy and actor load without fencing the stack address. */
    __asm__("" : "+m"(sp10) : : "memory");
    sp20 = Gp_ActorSlots[0];
    if (sp20 == NULL) {
        Task_CallExit(arg0);
        return;
    }
    if (ACTOR_FIELD(ACTOR_FIELD(ACTOR_FIELD(arg0, s32*, 0x20), s32*, 0x1C), s16*, 0x742) == 1) {
        Task_CallExit(arg0);
        return;
    }
    temp_v0            = SCRATCH_HEAD(void);
    temp_v0           -= 0x54;
    SCRATCH_HEAD(void) = temp_v0;
    temp_v0_2          = temp_v0;
    if (ACTOR_FIELD(arg0, s32*, 0x30) == 0) {
        temp_v0_3 = memCalloc(0x15C, 0);
        if (temp_v0_3 == NULL) {
            Task_CallExit(arg0);
            SCRATCH_POP_BYTES(0x54);
            return;
        }
        ACTOR_FIELD(arg0, s32*, 0x1C)    = temp_v0_3;
        ACTOR_FIELD(temp_s6, s32*, 0x4C) = &gGfxViewCoord;
        ACTOR_FIELD(temp_s6, s32*, 0x20) = 0;
        ACTOR_FIELD(temp_s6, s32*, 0x1C) = 0;
        ACTOR_FIELD(temp_s6, s32*, 0x18) = 0;
        ACTOR_FIELD(temp_s6, s32*, 0x0)  = 0;
        temp_s4                          = ACTOR_FIELD(arg0, s32*, 0x20);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        SOFT_USE_REG(temp_s4);
        ACTOR_FIELD(arg0, u16*, 0x2A) = 1U;
        ACTOR_FIELD(arg0, u8*, 0x38)  = 1U;
        ACTOR_FIELD(arg0, s32*, 0x3C) = 0;
        if (temp_s4 == NULL) {
            ACTOR_FIELD(temp_v0_3, s16*, 0x100) = 0;
            ACTOR_FIELD(temp_v0_3, s16*, 0x102) = -0x1000;
            ACTOR_FIELD(temp_v0_3, s16*, 0x104) = 0;
        } else {
            temp_s2                             = (void*)ACTOR_FIELD(temp_s4, s32*, 0x1C);
            ACTOR_FIELD(temp_v0_3, s16*, 0x102) = -0x1B8;
            firstVector                         = &sp18;
            temp_s1                             = temp_v0_3 + 0x100;

            ACTOR_FIELD(temp_v0_3, s16*, 0x100) = 0;
            ACTOR_FIELD(temp_v0_3, s16*, 0x104) = 0x4B0;
            sp18                                = *(SVECTOR*)(temp_v0_3 + 0x100);
            temp_a0                             = temp_s2 + 0x4B8;
            temp_s2                            += 0x4BC;
            gte_SetRotMatrix(temp_s2);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);
            ACTOR_FIELD(temp_s6, s32*, 0x18)    = (s32)(ACTOR_FIELD(temp_a0, s32*, 0x18) + ACTOR_FIELD(temp_v0_3, s16*, 0x100));
            ACTOR_FIELD(temp_s6, s32*, 0x1C)    = (s32)(ACTOR_FIELD(temp_a0, s32*, 0x1C) + ACTOR_FIELD(temp_v0_3, s16*, 0x102));
            ACTOR_FIELD(temp_s6, s32*, 0x20)    = (s32)(ACTOR_FIELD(temp_a0, s32*, 0x20) + ACTOR_FIELD(temp_v0_3, s16*, 0x104));
            ACTOR_FIELD(temp_v0_3, s16*, 0x100) = (s16)((rand() & 0x1FF) - 0x100);
            ACTOR_FIELD(temp_v0_3, s16*, 0x102) = (s16)((rand() & 0x1FF) - 0x100);
            ACTOR_FIELD(temp_v0_3, s16*, 0x104) = 0x1000;
            sp18                                = *(SVECTOR*)(temp_v0_3 + 0x100);
            gte_SetRotMatrix(temp_s2);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);
            SOFT_USE_REG2(firstVector, temp_s1);
            SOFT_USE_REG2(firstVector, temp_s1);
            SOFT_USE_REG2(firstVector, temp_s1);
            SOFT_USE_REG(firstVector);

            if (ACTOR_FIELD(arg0, s32*, 0x34) == 0x1100) {
                Gp_CopyCoordOffset(arg0, ACTOR_FIELD(ACTOR_FIELD(temp_s4, s32*, 0x2C), s32*, 0x8) + 0x460, (void*)var_fp);
            } else {
                Gp_CopyCoordOffset(arg0, ACTOR_FIELD(ACTOR_FIELD(temp_s4, s32*, 0x2C), s32*, 0x8) + 0x5A0, (void*)var_fp);
            }
            if (ACTOR_FIELD(arg0, s32*, 0x34) < 0x1000) {
                Gp_SpawnEff(0x601BB, temp_s6, 0x20, 0);
            }
            ACTOR_FIELD(arg0, u8*, 0x38)  = 3U;
            ACTOR_FIELD(arg0, u16*, 0x2A) = 0x20U;
        }
        var_s4 = 0;
        if (ACTOR_FIELD(arg0, s32*, 0x34) >= 0x1000) {
            ACTOR_FIELD(arg0, u8*, 0x38) = 4U;
        }
        do {
            ((SVECTOR*)temp_v0_3)[var_s4].vx = (u16)ACTOR_FIELD(temp_s6, s32*, 0x18);
            ((SVECTOR*)temp_v0_3)[var_s4].vy = (u16)ACTOR_FIELD(temp_s6, s32*, 0x1C);
            ((SVECTOR*)temp_v0_3)[var_s4].vz = (u16)ACTOR_FIELD(temp_s6, s32*, 0x20);
            var_s4                          += 1;
        } while (var_s4 < 0x20);
        temp_s1_2 = temp_v0_3 + 0x128;
        if (ACTOR_FIELD(arg0, s32*, 0x34) < 0x1000) {
            temp_s0                             = temp_v0_3 + 0x108;
            ACTOR_FIELD(temp_s0, s32*, 0x8)     = temp_s6;
            ACTOR_FIELD(temp_s0, s32*, 0xC)     = temp_s1_2;
            ACTOR_FIELD(temp_s0, s16*, 0x10)    = 0;
            ACTOR_FIELD(temp_s0, s16*, 0x12)    = 0;
            ACTOR_FIELD(temp_s0, s16*, 0x14)    = 0;
            ACTOR_FIELD(temp_s0, s16*, 0x1C)    = 0;
            temp_a0_2                           = temp_v0_3 + 0x140;
            ACTOR_FIELD(temp_s0, s32*, 0x18)    = Gp_PackPair(&D_actor_403600_801420F0, ACTOR_FIELD(arg0, s32*, 0x34) & 0xF);
            ACTOR_FIELD(temp_s0, u16*, 0x1E)    = 3U;
            ACTOR_FIELD(temp_s1_2, s32*, 0x14)  = temp_a0_2;
            ACTOR_FIELD(temp_s1_2, s16*, 0x8)   = 0;
            ACTOR_FIELD(temp_s1_2, s16*, 0xA)   = 0;
            ACTOR_FIELD(temp_s1_2, s16*, 0xC)   = 0;
            ACTOR_FIELD(temp_v0_3, s16*, 0x128) = 0;
            ACTOR_FIELD(temp_s1_2, s16*, 0x2)   = 0;
            ACTOR_FIELD(temp_s1_2, s16*, 0x4)   = 0;
            ACTOR_FIELD(temp_s1_2, s16*, 0x10)  = 0xC8;
            ACTOR_FIELD(temp_s1_2, s16*, 0x12)  = 0xC8;
            Gp_InitRec18Table(temp_a0_2, 1, 0);
            Gp_LinkObj(3, temp_s0);
            ACTOR_FIELD(temp_s0, u16*, 0x1E) = (u16)(ACTOR_FIELD(temp_s0, u16*, 0x1E) | 0xC000);
            ACTOR_FIELD(arg0, s32*, 0x18)    = &func_actor_403600_80138C68;
        }
        ACTOR_FIELD(temp_v0_3, s32*, 0x158) = 0x12C;
        ACTOR_FIELD(arg0, s32*, 0x30)       = (s32)(ACTOR_FIELD(arg0, s32*, 0x30) + 1);
        goto block_22;
    }
block_22:
    temp_s5   = ACTOR_FIELD(arg0, s32*, 0x1C);
    temp_s1_3 = ACTOR_FIELD(ACTOR_FIELD(sp20, s32*, 0x2C), s32*, 0x8) + 0x50;
    Gp_UpdateCoord(temp_s1_3);
    TransposeMatrix(&Gfx_ViewWorldMtx, temp_v0_2 + 0x34);
    temp_s0_2                          = (u8*)&Gfx_ViewWorldMtx - 0x24;
    var_s4                             = ACTOR_FIELD(temp_s1_3, s32*, 0x38) - ACTOR_FIELD(temp_s0_2, s32*, 0x38);
    ACTOR_FIELD(temp_v0_2, s16*, 0x10) = (s16)var_s4;
    var_s4                             = ACTOR_FIELD(temp_s1_3, s32*, 0x3C) - ACTOR_FIELD(temp_s0_2, s32*, 0x3C);
    ACTOR_FIELD(temp_v0_2, s16*, 0x12) = (s16)var_s4;
    temp_s1_4                          = temp_s5 + 0x128;
    cameraVector                       = &sp18;
    temp_v1                            = temp_v0_2 + 0x10;
    var_s4                             = ACTOR_FIELD(temp_s1_3, s32*, 0x40) - ACTOR_FIELD(temp_s0_2, s32*, 0x40);
    ACTOR_FIELD(temp_v0_2, s16*, 0x14) = (s16)var_s4;
    *cameraVector                      = *(SVECTOR*)(temp_v0_2 + 0x10);
    gteValue1                          = temp_v0_2 + 0x34;
    gte_SetRotMatrix(gteValue1);
    gte_ldv0(cameraVector);
    gte_rtv0();
    gte_stsv(temp_v1);
    ACTOR_FIELD(temp_v0_2, s32*, 0x0) = (s32)ACTOR_FIELD(temp_v0_2, s16*, 0x10);
    ACTOR_FIELD(temp_v0_2, s32*, 0x4) = (s32)ACTOR_FIELD(temp_v0_2, s16*, 0x12);
    ACTOR_FIELD(temp_v0_2, s32*, 0x8) = (s32)ACTOR_FIELD(temp_v0_2, s16*, 0x14);
    if (Gp_StateF0.field_4 == 0) {
        if (ACTOR_FIELD(arg0, s32*, 0x34) < 0x1000) {
            ACTOR_FIELD(temp_s5, s32*, 0x158) = (s32)(ACTOR_FIELD(temp_s5, s32*, 0x158) - 1);
        } else {
            ACTOR_FIELD(arg0, u16*, 0x2A) = (u16)(ACTOR_FIELD(arg0, u16*, 0x2A) + 1);
        }
        var_s4 = 0;
        do {
            historyDst  = (31 - var_s4) * 8;
            historyOut  = (SVECTOR*)((s32)temp_s5 + historyDst);
            historySrc  = (30 - var_s4) * 8;
            *historyOut = *(SVECTOR*)((s32)temp_s5 + historySrc);
            var_s4     += 1;
        } while (var_s4 < 0x1F);
        ACTOR_FIELD(arg0, s32*, 0x3C) ^= 1;
        temp_v1_3                      = ACTOR_FIELD(arg0, u16*, 0x2A) - 1;
        ACTOR_FIELD(arg0, u16*, 0x2A)  = temp_v1_3;
        if ((temp_v1_3 << 0x10) <= 0) {
            temp_v1_4 = ACTOR_FIELD(arg0, u8*, 0x38);
            if (temp_v1_4 == 2) {
                Task_CallExit(arg0);
            } else {
                if (temp_v1_4 == 3) {
                    temp_s0_3 = (s8)Gp_GetObjPan(temp_s6);
                    SndEvt_EnqueueType6(0x54160009, temp_s0_3, (s8)gpGetObjDepth(temp_s6));
                    var_v0 = (rand() & 0xF) + 0x10;
                    goto block_34;
                }
                var_v0 = 0x7FFF;
                if (temp_v1_4 == 0) {
                block_34:
                    ACTOR_FIELD(arg0, u16*, 0x2A) = var_v0;
                    ACTOR_FIELD(arg0, u8*, 0x38)  = 1U;
                } else {
                    ACTOR_FIELD(arg0, u16*, 0x2A) = (u16)((rand() & 0xF) + 0x10);
                    ACTOR_FIELD(arg0, u8*, 0x38)  = (u8)(ACTOR_FIELD(arg0, u8*, 0x38) ^ 1);
                }
                goto block_36;
            }
        } else {
        block_36:
            if (ACTOR_FIELD(arg0, u8*, 0x38) != 2) {
                temp_s2   = 0;
                temp_s0_4 = temp_v0_2 + 0x10;
                do {
                    if (ACTOR_FIELD(arg0, u8*, 0x38) == 0) {
                        ACTOR_FIELD(temp_v0_2, s16*, 0x10) = (s16)((ACTOR_FIELD(temp_v0_2, s32*, 0x0) - ACTOR_FIELD(temp_s6, s32*, 0x18)) >> 2);
                        ACTOR_FIELD(temp_v0_2, s16*, 0x12) = (s16)((s32)(ACTOR_FIELD(temp_v0_2, s32*, 0x4) - ACTOR_FIELD(temp_s6, s32*, 0x1C)) >> 2);
                        ACTOR_FIELD(temp_v0_2, s16*, 0x14) = (s16)((s32)(ACTOR_FIELD(temp_v0_2, s32*, 0x8) - ACTOR_FIELD(temp_s6, s32*, 0x20)) >> 2);
                        VectorNormalSS(temp_s0_4, temp_s0_4);
                        /* Keep scratch loads live across the weighted shifts. */
                        velocityX = ACTOR_FIELD(temp_s5, s16*, 0x100);
                        weightedX = velocityX * 8;
                        scratchX  = ACTOR_FIELD(temp_v0_2, s16*, 0x10);
                        SOFT_TOUCH_REG_USE(weightedX, scratchX);
                        weightedX                         -= velocityX;
                        ACTOR_FIELD(temp_v0_2, s16*, 0x10) = (scratchX + weightedX) >> 4;
                        velocityY                          = ACTOR_FIELD(temp_s5, s16*, 0x102);
                        weightedY                          = velocityY * 8;
                        scratchY                           = ACTOR_FIELD(temp_v0_2, s16*, 0x12);
                        SOFT_TOUCH_REG_USE(weightedY, scratchY);
                        weightedY                         -= velocityY;
                        ACTOR_FIELD(temp_v0_2, s16*, 0x12) = (scratchY + weightedY) >> 4;
                        velocityZ                          = ACTOR_FIELD(temp_s5, s16*, 0x104);
                        weightedZ                          = velocityZ * 8;
                        scratchZ                           = ACTOR_FIELD(temp_v0_2, s16*, 0x14);
                        SOFT_TOUCH_REG_USE(weightedZ, scratchZ);
                        weightedZ                         -= velocityZ;
                        ACTOR_FIELD(temp_v0_2, s16*, 0x14) = (scratchZ + weightedZ) >> 4;
                        VectorNormalSS(temp_s0_4, temp_s0_4);
                        ACTOR_FIELD(temp_s5, s16*, 0x100) = (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x10);
                        ACTOR_FIELD(temp_s5, s16*, 0x102) = (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x12);
                        ACTOR_FIELD(temp_s5, s16*, 0x104) = (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x14);
                    }
                    temp_v1_5 = ACTOR_FIELD(arg0, u8*, 0x38);
                    if (temp_v1_5 < 3U) {
                        gte_lddp(100);
                        gteValue2 = temp_s5 + 0x100;
                        gte_ldsv(gteValue2);
                        gte_gpf12();
                        gte_stsv(temp_s0_4);
                        ACTOR_FIELD(temp_s1_4, s16*, 0x8) = (s16) - (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x10);
                        ACTOR_FIELD(temp_s1_4, s16*, 0xA) = (s16) - (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x12);
                        ACTOR_FIELD(temp_s1_4, s16*, 0xC) = (s16) - (s16)(u16)ACTOR_FIELD(temp_v0_2, s16*, 0x14);
                        ACTOR_FIELD(temp_s6, s32*, 0x18)  = (s32)(ACTOR_FIELD(temp_s6, s32*, 0x18) + ACTOR_FIELD(temp_v0_2, s16*, 0x10));
                        ACTOR_FIELD(temp_s6, s32*, 0x1C)  = (s32)(ACTOR_FIELD(temp_s6, s32*, 0x1C) + ACTOR_FIELD(temp_v0_2, s16*, 0x12));
                        ACTOR_FIELD(temp_s6, s32*, 0x20)  = (s32)(ACTOR_FIELD(temp_s6, s32*, 0x20) + ACTOR_FIELD(temp_v0_2, s16*, 0x14));
                        goto block_51;
                    }
                    if (temp_v1_5 == 3) {
                        motionParent = ACTOR_FIELD(arg0, s32*, 0x20);
                        if ((s16)ACTOR_FIELD(arg0, u16*, 0x2A) >= 7) {
                            var_a1_2 = ACTOR_FIELD(ACTOR_FIELD(motionParent, s32*, 0x2C), s32*, 0x8) + 0x5A0;
                            Gp_CopyCoordOffset(arg0, var_a1_2, &sp10);
                            var_s4 = 0;
                            do {
                                ((SVECTOR*)temp_s5)[var_s4].vx = (u16)ACTOR_FIELD(temp_s6, s32*, 0x18);
                                ((SVECTOR*)temp_s5)[var_s4].vy = (u16)ACTOR_FIELD(temp_s6, s32*, 0x1C);
                                ((SVECTOR*)temp_s5)[var_s4].vz = (u16)ACTOR_FIELD(temp_s6, s32*, 0x20);
                                var_s4                        += 1;
                            } while (var_s4 < 0x20);
                            temp_s2 += 1;
                        } else {
                            goto block_51;
                        }
                    } else {
                        temp_a0_3 = ACTOR_FIELD(arg0, s32*, 0x20);
                        if (ACTOR_FIELD(arg0, s32*, 0x34) == 0x1000) {
                            Gp_CopyCoordOffset(arg0, ACTOR_FIELD(ACTOR_FIELD(temp_a0_3, s32*, 0x2C), s32*, 0x8) + 0x5A0, &sp10);
                        } else {
                            Gp_CopyCoordOffset(arg0, ACTOR_FIELD(ACTOR_FIELD(temp_a0_3, s32*, 0x2C), s32*, 0x8) + 0x460, &sp10);
                        }
                    block_51:
                        temp_s2 += 1;
                    }
                } while (temp_s2 < 2);
            }
            ACTOR_FIELD(temp_s6, s32*, 0x0) = 0;
            Gp_UpdateCoord(temp_s6);
            ACTOR_FIELD(temp_s5, u16*, 0x0) = (u16)ACTOR_FIELD(temp_s6, s32*, 0x18);
            ACTOR_FIELD(temp_s5, u16*, 0x2) = (u16)ACTOR_FIELD(temp_s6, s32*, 0x1C);
            ACTOR_FIELD(temp_s5, u16*, 0x4) = (u16)ACTOR_FIELD(temp_s6, s32*, 0x20);
            ACTOR_FIELD(temp_s5, u16*, 0x6) = rand();
            goto block_54;
        }
    } else {
    block_54:
        if ((ACTOR_FIELD(arg0, s32*, 0x34) < 0x1000) && (Gp_FindRec18(temp_s5 + 0x140, 0) != 0)) {
            temp_s0_5 = (s8)Gp_GetObjPan(temp_s6);
            SndEvt_EnqueueType6(0x5416000A, temp_s0_5, (s8)gpGetObjDepth(temp_s6));
            ACTOR_FIELD(temp_s5, s32*, 0x158) = -1;
        }
        if (ACTOR_FIELD(temp_s5, s32*, 0x158) < 0) {
            if (ACTOR_FIELD(arg0, s32*, 0x34) < 0x1000) {
                Gp_ClearRec18Occupied(temp_s5 + 0x140);
                ACTOR_FIELD(temp_s5, u16*, 0x126) = (u16)(ACTOR_FIELD(temp_s5, u16*, 0x126) & 0x3FFF);
            }
            ACTOR_FIELD(temp_s5, s32*, 0x158) = 0x7FFFFFFF;
            ACTOR_FIELD(arg0, u8*, 0x38)      = 2U;
            ACTOR_FIELD(arg0, u16*, 0x2A)     = 0x20U;
        }
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        temp_v1_6 = ACTOR_FIELD(arg0, s32*, 0x34) & 0xF;
        switch (temp_v1_6) {
            case 0:
                sp24 = 0x808000;
                break;
            case 1:
                sp24 = 0x808080;
                break;
            case 2:
                sp24 = 0x80;
                break;
            default:
                sp24 = 0x8080;
                break;
        }
        sp28      = 1;
        temp_v1_7 = ACTOR_FIELD(arg0, u8*, 0x38);
        var_fp    = 0;
        if (temp_v1_7 == 2) {
            var_fp = 0x20 - (s16)ACTOR_FIELD(arg0, u16*, 0x2A);
        } else if (temp_v1_7 == 3) {
            sp28 = 4;
        }
        var_s4 = var_fp;
        if (var_s4 < 0x20) {
            var_a1_3 = (s32*)&gDisplayState;
            SOFT_USE_REG(var_a1_3);
            temp_s6 = (void*)0xFFFFFF;
            var_a2  = 0xFF000000;
            SOFT_USE_REG(var_a2);
            SOFT_USE_REG(var_a2);
            SOFT_USE_REG(var_a2);
            SOFT_USE_REG(var_a2);

            var_s1 = (void*)((var_s4 * 8) + (s32)temp_s5);
            do {
                temp_s2                 = D_actor_403600_8016069C;
                D_actor_403600_8016069C = temp_s2 + 0x28;
                gte_ldv0(var_s1);
                gte_rtps();
                gteValue3 = temp_v0_2 + 0x18;
                gte_stsxy(gteValue3);
                gteValue4 = temp_v0_2 + 0x1C;
                gte_stdp(gteValue4);
                gteValue5 = temp_v0_2 + 0x20;
                gte_stflg(gteValue5);
                gteValue6 = temp_v0_2 + 0x24;
                gte_stszotz(gteValue6);
                if (ACTOR_FIELD(temp_v0_2, s32*, 0x20) >= 0) {
                    if (var_s4 == 0) {
                        if (ACTOR_FIELD(arg0, u8*, 0x38) != 2) {
                            temp_a0_4 = ACTOR_FIELD(temp_v0_2, s32*, 0x24);
                            if (temp_a0_4 >= 0) {
                                ACTOR_FIELD(temp_v0_2, u16*, 0x2C) = (u16)((s32)(ACTOR_FIELD(var_a1_3, u16*, 0x110) * 0x96) / temp_a0_4);
                            } else {
                                ACTOR_FIELD(temp_v0_2, u16*, 0x2C) = 0x1000U;
                            }
                            temp_v1_8                        = ACTOR_FIELD(temp_v0_2, u16*, 0x18) - ACTOR_FIELD(temp_v0_2, u16*, 0x2C);
                            ACTOR_FIELD(temp_s2, s16*, 0x18) = temp_v1_8;
                            ACTOR_FIELD(temp_s2, s16*, 0x8)  = temp_v1_8;
                            temp_v1_9                        = ACTOR_FIELD(temp_v0_2, u16*, 0x18) + ACTOR_FIELD(temp_v0_2, u16*, 0x2C);
                            ACTOR_FIELD(temp_s2, s16*, 0x20) = temp_v1_9;
                            ACTOR_FIELD(temp_s2, s16*, 0x10) = temp_v1_9;
                            temp_v1_10                       = ACTOR_FIELD(temp_v0_2, u16*, 0x1A) - ACTOR_FIELD(temp_v0_2, u16*, 0x2C);
                            ACTOR_FIELD(temp_s2, s16*, 0x12) = temp_v1_10;
                            ACTOR_FIELD(temp_s2, s16*, 0xA)  = temp_v1_10;
                            temp_v1_11                       = ACTOR_FIELD(temp_v0_2, u16*, 0x1A) + ACTOR_FIELD(temp_v0_2, u16*, 0x2C);
                            ACTOR_FIELD(temp_s2, s16*, 0x16) = 0x29;
                            ACTOR_FIELD(temp_s2, s16*, 0x22) = temp_v1_11;
                            ACTOR_FIELD(temp_s2, s16*, 0x1A) = temp_v1_11;
                            if (ACTOR_FIELD(arg0, s32*, 0x3C) != 0) {
                                ACTOR_FIELD(temp_s2, u8*, 0x1C) = 0x70U;
                                ACTOR_FIELD(temp_s2, u8*, 0xC)  = 0x70U;
                                ACTOR_FIELD(temp_s2, s16*, 0xE) = 0x428B;
                            } else {
                                ACTOR_FIELD(temp_s2, u8*, 0x1C) = 0xA8U;
                                ACTOR_FIELD(temp_s2, u8*, 0xC)  = 0xA8U;
                                ACTOR_FIELD(temp_s2, s16*, 0xE) = 0x428C;
                            }
                            ACTOR_FIELD(temp_s2, u8*, 0x15) = 0xC9;
                            ACTOR_FIELD(temp_s2, u8*, 0xD)  = 0xC9;
                            ACTOR_FIELD(temp_s2, u8*, 0x25) = 0xFF;
                            ACTOR_FIELD(temp_s2, u8*, 0x1D) = 0xFF;
                            ACTOR_FIELD(temp_s2, s32*, 0x4) = sp24;
                            temp_v1_12                      = ACTOR_FIELD(temp_s2, u8*, 0xC) + 0x37;
                            ACTOR_FIELD(temp_s2, u8*, 0x3)  = 9;
                            ACTOR_FIELD(temp_s2, u8*, 0x7)  = 0x2E;
                            ACTOR_FIELD(temp_s2, u8*, 0x24) = temp_v1_12;
                            ACTOR_FIELD(temp_s2, u8*, 0x14) = temp_v1_12;
                            goto block_100;
                        }
                    } else if (var_s4 >= (var_fp - 4)) {
                        temp_s0_6                          = ACTOR_FIELD(var_s1, u16*, 0x6);
                        sp30                               = var_a1_3;
                        sp34                               = var_a2;
                        ACTOR_FIELD(temp_v0_2, u16*, 0x2C) = rsin(temp_s0_6);
                        ACTOR_FIELD(temp_v0_2, u16*, 0x2E) = rcos(temp_s0_6);
                        ACTOR_FIELD(temp_v0_2, u16*, 0x30) = 0;
                        if (ACTOR_FIELD(temp_v0_2, s32*, 0x24) >= 0) {
                            var_a0 = (var_s4 * 2) + 0x78;
                            if ((var_fp >= var_s4) && (ACTOR_FIELD(arg0, u8*, 0x38) == 2)) {
                                var_a0 = (var_s4 * 20) + 0x78;
                            } else if (ACTOR_FIELD(arg0, u8*, 0x38) == 4) {
                                var_a0 *= 2;
                            }
                            gteValue7 = (var_a0 * ACTOR_FIELD(var_a1_3, u16*, 0x110)) / ACTOR_FIELD(temp_v0_2, s32*, 0x24);
                            gte_lddp(gteValue7);
                            temp_v0_4 = temp_v0_2 + 0x2C;
                            gte_ldsv(temp_v0_4);
                            gte_gpf12();
                            gte_stsv(temp_v0_4);
                        }
                        ACTOR_FIELD(temp_s2, s16*, 0x8)  = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x18) + ACTOR_FIELD(temp_v0_2, u16*, 0x2C));
                        ACTOR_FIELD(temp_s2, s16*, 0xA)  = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x1A) + ACTOR_FIELD(temp_v0_2, u16*, 0x2E));
                        ACTOR_FIELD(temp_s2, s16*, 0x10) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x18) + ACTOR_FIELD(temp_v0_2, u16*, 0x2E));
                        ACTOR_FIELD(temp_s2, s16*, 0x12) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x1A) - ACTOR_FIELD(temp_v0_2, u16*, 0x2C));
                        ACTOR_FIELD(temp_s2, s16*, 0x18) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x18) - ACTOR_FIELD(temp_v0_2, u16*, 0x2E));
                        ACTOR_FIELD(temp_s2, s16*, 0x1A) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x1A) + ACTOR_FIELD(temp_v0_2, u16*, 0x2C));
                        ACTOR_FIELD(temp_s2, s16*, 0x20) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x18) - ACTOR_FIELD(temp_v0_2, u16*, 0x2C));
                        ACTOR_FIELD(temp_s2, s16*, 0x22) = (s16)(ACTOR_FIELD(temp_v0_2, u16*, 0x1A) - ACTOR_FIELD(temp_v0_2, u16*, 0x2E));
                        temp_v1_13                       = (u8)ACTOR_FIELD(var_s1, u16*, 0x6) & 0x20;
                        ACTOR_FIELD(temp_s2, u8*, 0x15)  = 0x18;
                        ACTOR_FIELD(temp_s2, u8*, 0xD)   = 0x18;
                        ACTOR_FIELD(temp_s2, u8*, 0x25)  = 0x37;
                        ACTOR_FIELD(temp_s2, u8*, 0x1D)  = 0x37;
                        temp_v0_5                        = temp_v1_13 + 0x60;
                        temp_v1_13                      += 0x7F;
                        ACTOR_FIELD(temp_s2, u8*, 0x1C)  = temp_v0_5;
                        ACTOR_FIELD(temp_s2, u8*, 0xC)   = temp_v0_5;
                        ACTOR_FIELD(temp_s2, u8*, 0x24)  = temp_v1_13;
                        ACTOR_FIELD(temp_s2, u8*, 0x14)  = temp_v1_13;
                        ACTOR_FIELD(temp_s2, s16*, 0x16) = 0x2A;
                        ACTOR_FIELD(temp_s2, s16*, 0xE)  = 0x42CC;
                        var_a0                           = var_s4;
                        if (ACTOR_FIELD(arg0, u8*, 0x38) == 2) {
                            if (var_fp >= var_s4) {
                                var_a0 = var_fp;
                            }
                        }
                        temp_v1_13                      = D_actor_403600_80142120[var_a0];
                        ACTOR_FIELD(temp_s2, u8*, 0x3)  = 9;
                        ACTOR_FIELD(temp_s2, s32*, 0x4) = temp_v1_13;
                        ACTOR_FIELD(temp_s2, u8*, 0x7)  = 0x2E;
                    block_100:
                        otOffset                        = (((u32)(ACTOR_FIELD(temp_v0_2, s32*, 0x24) << ACTOR_FIELD(var_a1_3, u8*, 0x128)) >> 2) & 0xFFC);
                        ot                              = (s32*)gGpuCurrentOt;
                        ACTOR_FIELD(temp_s2, s32*, 0x0) = (ACTOR_FIELD(temp_s2, s32*, 0x0) & var_a2) |
                                                          (*(s32*)(otOffset + (s32)ot) & (s32)temp_s6);
                        temp_v0_6  = (s32*)((((u32)(ACTOR_FIELD(temp_v0_2, s32*, 0x24) << ACTOR_FIELD(var_a1_3, u8*, 0x128)) >> 2) & 0xFFC) + (s32)ot);
                        *temp_v0_6 = (*temp_v0_6 & var_a2) | ((s32)temp_s2 & (s32)temp_s6);
                    }
                }
                SOFT_USE_REG(temp_s5);
                var_s1 += sp28 * 8;
                var_s4 += sp28;
            } while (var_s4 < 0x20);
        }
    }
    SCRATCH_POP_BYTES(0x54);
}

void func_actor_403600_801353D0(ActorEffectState* arg0, GsCOORDINATE2* arg1)
{
    s32                       radii[16];
    s32                       heights[16];
    s32                       corner[4];
    s32                       i;
    s32                       j;
    s32                       firstAngle;
    s32                       angle;
    s32                       index;
    s32                       value;
    s32                       firstRadius;
    s32                       rotation;
    s32                       mirrorXY;
    s32                       projectedX;
    s32                       projectedY;
    s32                       screenX;
    s32                       screenY;
    s32                       min;
    s32                       max;
    s32                       adjust;
    s32                       radiusOffset;
    s32                       scale;
    s32                       scanCount;
    s32                       otz;
    s32*                      height;
    MATRIX*                   matrix;
    s32*                      heightBase;
    s32*                      scan;
    u32                       mask;
    u32                       maskHi;
    u_long*                   ot;
    u16                       oldY;
    u8*                       head;
    u8*                       newHead;
    u8*                       uv;
    u8*                       previous;
    u8*                       mirror;
    POLY_FT4*                 poly;
    SVECTOR*                  vec;
    Actor403600EffectScratch* scratch;

    head               = SCRATCH_HEAD(u8);
    newHead            = head - 0x78;
    SCRATCH_HEAD(void) = newHead;
    scratch            = (Actor403600EffectScratch*)newHead;
    Gp_UpdateCoord(arg1);
    gte_SetRotMatrix(&arg1->workm);
    gte_SetTransMatrix(&arg1->workm);

    scratch->points[1].vz = 0x1000;
    scratch->points[2].vx = 0x1000;
    scratch->points[0].vx = 0;
    scratch->points[0].vy = 0;
    scratch->points[0].vz = 0;
    scratch->points[1].vx = 0;
    scratch->points[1].vy = 0;
    scratch->points[2].vy = 0;
    scratch->points[2].vz = 0;
    gte_ldv3(&scratch->points[0], &scratch->points[1], &scratch->points[2]);
    gte_rtpt();
    gte_stsxy3(&scratch->sxy3[0], &scratch->sxy3[1], &scratch->sxy3[2]);
    gte_stdp(&scratch->dp);
    gte_stflg(&scratch->flag);
    gte_stszotz(&scratch->otz);
    gte_nclip();
    gte_stopz(&scratch->nclip);

    i = 0;
    do {
        firstAngle  = arg0->field_80 + i * 2;
        index       = firstAngle % 32;
        firstRadius = (rsin(arg0->field_0[index]) * arg0->field_40[index]) >> 12;
        value       = (firstRadius * (16 - i)) / 16;
        firstRadius = value >> 3;
        if (arg0->field_E0 == 1) {
            firstRadius = value >> 8;
        }
        radii[i]   = firstRadius;
        value      = (arg0->field_40[index] * (15 - i)) >> 10;
        heights[i] = value;
        if (scratch->nclip > 0) {
            heights[i] = -value;
        }
        i++;
        j = 0;
    } while (i < 16);

    scratch->maxOtz = 0;
    matrix          = &scratch->matrix;
    vec             = &scratch->vec;
    heightBase      = heights;
    mask            = 0xFFFFFF;
    do {
        scratch->matrix = arg1->workm;
        Gfx_RotMatrixY(matrix, (j << 12) / 12, 0);
        gte_SetTransMatrix(&arg1->workm);
        gte_SetRotMatrix(matrix);
        scale        = 0x14;
        i            = 0;
        height       = heightBase;
        radiusOffset = 0;
        angle        = arg0->field_80;
        do {
            angle                   %= 32;
            poly                     = (POLY_FT4*)D_actor_403600_8016069C;
            D_actor_403600_8016069C += sizeof(POLY_FT4);
            rotation                 = -rcos(arg0->field_0[angle]) >> 3;
            scratch->vec.vx          = rsin(rotation);
            scratch->vec.vy          = rcos(rotation);
            scratch->vec.vz          = 0;
            gte_ldv0(vec);
            gte_rtv0();
            scratch->projected.vx = scale;
            scratch->projected.vz = 0;
            scratch->projected.vy = *(s32*)((u8*)radii + radiusOffset);
            gte_stsv(vec);
            gte_ldv0(&scratch->projected);
            gte_rtps();
            gte_stsxy(&scratch->sxy);
            gte_stdp(&scratch->dp);
            gte_stflg(&scratch->flag);
            gte_stszotz(&scratch->otz);
            gte_lddp(*height);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);

            *(s32*)&poly->x0 = scratch->sxy;
            oldY             = poly->y0;
            projectedX       = scratch->vec.vx + 0xA0;
            screenX          = (s16)poly->x0 + projectedX;
            projectedY       = scratch->vec.vy + 0x78;
            screenY          = (s16)poly->y0 + projectedY;
            if (screenY >= 0xF0) {
                poly->y0 = oldY + 0xEF - screenY;
                screenY  = 0xEF;
            } else if (screenY < 0) {
                poly->y0 = oldY - screenY;
                screenY  = 0;
            }
            if (screenX >= 0x140) {
                poly->x0 = (u16)poly->x0 + 0x13F - screenX;
            } else if (screenX < 0) {
                poly->x0 = (u16)poly->x0 - screenX;
                screenX  = 0;
            }
            ((u8*)poly)[0x1E] = 0;
            if (screenX >= 0x100) {
                ((u8*)poly)[0x1E] = 0x40;
            }
            poly->v0 = screenY;
            poly->u0 = screenX - ((u8*)poly)[0x1E];
            if (scratch->flag >= 0 && i != 15 && (*height != 0 || (index = i + 1, index *= 4, *(s32*)((s32)heightBase + index) != 0))) {
                ((u8*)poly)[3] = 9;
                ((u8*)poly)[7] = 0x2D;
                scratch->otz   = (scratch->otz << D_80071090 & 0x3FFF) >> 4;
                if (scratch->maxOtz < scratch->otz) {
                    scratch->maxOtz = scratch->otz;
                }
                ot     = gGpuCurrentOt;
                otz    = scratch->otz;
                maskHi = 0xFF000000;
                SOFT_TOUCH_REG_USE(maskHi, otz);
                poly->tag        = (poly->tag & maskHi) | (ot[otz] & mask);
                ot[scratch->otz] = (ot[scratch->otz] & maskHi) | ((u32)poly & mask);
            }
            previous = (u8*)poly - sizeof(POLY_FT4);
            if (i != 0) {
                *(s32*)(previous + 0x10) = *(s32*)&poly->x0;
                previous[0x14]           = poly->u0;
                do {
                    previous[0x15] = poly->v0;
                    previous[0x1F] = ((u8*)poly)[0x1E];
                    if (j != 0) {
                        mirrorXY = *(s32*)(previous + 0x08);
                        mirror   = (u8*)poly - 0x2A8;
                    } else {
                        mirrorXY = *(s32*)(previous + 0x08);
                        mirror   = (u8*)poly + 0x1B58;
                    }
                    *(s32*)(mirror + 0x18) = mirrorXY;
                    mirror[0x1C]           = previous[0x0C];
                } while (0);
                mirror[0x1D]           = previous[0x0D];
                mirror[0x26]           = previous[0x1E];
                *(s32*)(mirror + 0x20) = *(s32*)(previous + 0x10);
                mirror[0x24]           = previous[0x14];
                mirror[0x25]           = previous[0x15];
                mirror[0x27]           = previous[0x1F];
            }
            height++;
            radiusOffset += 4;
            i++;
            angle += 2;
            scale += 0x9B;
        } while (i < 16);
        j++;
    } while (j < 12);

    j = 0;
    do {
        i  = 0;
        uv = (u8*)poly + 0x28;
        do {
            corner[0] = uv[-28] + uv[-10];
            corner[1] = uv[-20] + uv[-9];
            corner[2] = uv[-12] + uv[-2];
            corner[3] = uv[-4] + uv[-1];
            min       = corner[0];
            max       = corner[0];
            for (scanCount = 1; scanCount < 4; scanCount++) {
                if (corner[scanCount] < min) {
                    min = corner[scanCount];
                } else if (max < corner[scanCount]) {
                    max = corner[scanCount];
                }
            }
            if (max >= 0x100 || min >= 0x40) {
                adjust = 0x40;
            } else {
                adjust = 0;
            }
            *(u16*)(uv - 0x12) = ((u32)(adjust + 0x1C0) >> 6) | 0x110;
            uv[-28]            = corner[0] - adjust;
            uv[-20]            = corner[1] - adjust;
            poly--;
            uv[-12] = corner[2] - adjust;
            i++;
            uv[-4] = corner[3] - adjust;
            uv    -= sizeof(POLY_FT4);
        } while (i < 16);
        j++;
    } while (j < 12);
    func_actor_403600_801320F8(scratch->maxOtz + 1);
    SCRATCH_POP_BYTES(0x78);
}

const SVECTOR D_actor_403600_80131E2C = { 0, 0x578, 0, 0 };

void func_actor_403600_80135C28(Task* arg0)
{
    SVECTOR           sp10;
    s16*              temp_a0_2;
    s16*              temp_a0_6;
    ActorEffectState* temp_s0;
    s16*              temp_v0_11;
    s16*              temp_v0_13;
    ActorEffectState* temp_v0_2;
    s16*              temp_v0_4;
    s16*              temp_v0_7;
    s16*              temp_v1_5;
    s16*              temp_v1_8;
    s16*              var_a0_2;
    GsCOORDINATE2*    temp_s4;
    s32               temp_a0_3;
    s32               temp_a0_4;
    s32               temp_a0_7;
    s32               temp_v0_10;
    s32               temp_v0_12;
    s32               temp_v0_3;
    s32               temp_v0_5;
    s32               temp_v0_6;
    s32               temp_v0_8;
    s32               temp_v0_9;
    s32               temp_v1_10;
    s32               temp_v1_11;
    s32               temp_v1_12;
    s32               temp_v1_2;
    s32               temp_v1_3;
    s32               temp_v1_4;
    s32               temp_v1_6;
    s32               temp_v1_7;
    s32               temp_v1_9;
    s32               var_a1;
    s32               var_a1_2;
    s32               var_v0;
    s32               var_v0_2;
    s32               var_v0_3;
    s32               var_v0_4;
    s32               var_v0_5;
    s32               var_v0_6;
    s32               var_v0_7;
    s32               var_v1;
    Task*             temp_a0;
    TmdObject*        temp_a0_5;
    TmdObject*        temp_a1;
    Task*             temp_s2;
    TmdObject*        temp_v0;
    Actor403600Work*  temp_v1;

    temp_a0 = arg0->spawnArg2;
    temp_v1 = temp_a0->work;
    temp_s4 = ((TmdObject*)arg0->extra)->coords;
    temp_s2 = temp_v1->field_710;
    if (temp_v1->field_742 == 1) {
        temp_v0                 = temp_a0->extra;
        D_actor_403600_801606A0 = NULL;
        temp_v0->flags          = (u16)(temp_v0->flags & 0xFF7F);
        Task_CallExit(arg0);
        return;
    }
    if (arg0->state == 0) {
        temp_v0_2 = memCalloc(0xE8, 0);
        if (temp_v0_2 != NULL) {
            arg0->work          = (TaskIdMap*)temp_v0_2;
            temp_v0_2->field_E0 = 0;
            sp10                = D_actor_403600_80131E2C;
            Gp_CopyCoordOffset(arg0, &((TmdObject*)temp_s2->parent->extra)->coords[1], &sp10);
            temp_a0_2                      = &temp_v0_2->field_90.coord.m[0][0];
            *(s32*)&temp_a0_2[0]           = 0x1000;
            *(s32*)&temp_a0_2[2]           = 0;
            *(s32*)&temp_a0_2[4]           = 0x1000;
            *(s32*)&temp_a0_2[6]           = 0;
            temp_a0_2[8]                   = 0x1000;
            temp_v0_2->field_90.coord.t[0] = 0;
            temp_v0_2->field_90.coord.t[1] = 0;
            temp_v0_2->field_90.coord.t[2] = 0;
            temp_v0_2->field_90.flg        = 0;
            temp_v0_2->field_90.sub        = temp_s4;
            temp_v1_2                      = arg0->spawnArg1;
            arg0->killCountdown            = 0x10;
            switch (temp_v1_2) {
                case 1:
                    temp_v0_2->field_8E = (s16)temp_v1_2;
                    var_a1_2            = 0;
                    do {
                        temp_a0_4           = temp_v0_2->field_80;
                        temp_v1_6           = temp_a0_4 + 0x1F;
                        var_v0_3            = temp_v1_6;
                        temp_v0_2->field_80 = temp_v1_6;
                        if (temp_v1_6 < 0) {
                            var_v0_3 = temp_a0_4 + 0x3E;
                        }
                        temp_v0_6 = temp_v1_6 - ((var_v0_3 >> 5) << 5);
                        __asm__("move %0,%1" : "=r"(temp_a0_4) : "r"(temp_v0_6));
                        temp_v0_2->field_80 = temp_v0_6;
                        temp_v0_7           = &temp_v0_2->field_0[temp_a0_4];
                        temp_v0_7[0]        = 0;
                        temp_v0_7[0x20]     = 0U;
                        if (temp_v0_2->field_8E != 0) {
                            if (temp_v0_2->field_8C == 0) {
                                temp_v0_2->field_84 = 0;
                            }
                            temp_v1_7 = temp_v0_2->field_88;
                            if (temp_v1_7 < 0x1000) {
                                temp_v0_2->field_88 = (s32)(temp_v1_7 + 0x200);
                            }
                        } else {
                            temp_v0_8 = temp_v0_2->field_88;
                            if (temp_v0_8 > 0) {
                                temp_v0_2->field_88 = (s32)(temp_v0_8 - 0x80);
                            }
                        }
                        temp_v0_2->field_8C = (s16)(u16)temp_v0_2->field_8E;
                        if (temp_v0_2->field_88 != 0) {
                            temp_v1_8       = &temp_v0_2->field_0[temp_a0_4];
                            temp_v1_8[0]    = (s16)(u16)temp_v0_2->field_84;
                            temp_v1_8[0x20] = (u16)temp_v0_2->field_88;
                            if (temp_v0_2->field_E0 == 0) {
                                var_v0_4 = temp_v0_2->field_84 + 0x180;
                            } else {
                                var_v0_4 = temp_v0_2->field_84 + 0x100;
                            }
                            temp_v0_2->field_84 = var_v0_4;
                        }
                        var_a1_2 += 1;
                    } while (var_a1_2 < 0x10);
                    temp_v0_2->field_E4 = 8;
                    break;
                case 2:
                    arg0->killCountdown = 0x2E;
                    temp_v0_2->field_E4 = 0x1F;
                    Gfx_RotMatrixZ(temp_a0_2, 0x800, 0);
                    temp_s4->flg = 0;
                    break;
                default:
                    temp_v0_2->field_8E = 1;
                    var_a1              = 0;
                    do {
                        temp_a0_3           = temp_v0_2->field_80;
                        temp_v1_3           = temp_a0_3 + 0x1F;
                        var_v0              = temp_v1_3;
                        temp_v0_2->field_80 = temp_v1_3;
                        if (temp_v1_3 < 0) {
                            var_v0 = temp_a0_3 + 0x3E;
                        }
                        temp_v0_3 = temp_v1_3 - ((var_v0 >> 5) << 5);
                        __asm__("move %0,%1" : "=r"(temp_a0_3) : "r"(temp_v0_3));
                        temp_v0_2->field_80 = temp_v0_3;
                        temp_v0_4           = &temp_v0_2->field_0[temp_a0_3];
                        temp_v0_4[0]        = 0;
                        temp_v0_4[0x20]     = 0U;
                        if (temp_v0_2->field_8E != 0) {
                            if (temp_v0_2->field_8C == 0) {
                                temp_v0_2->field_84 = 0;
                            }
                            temp_v1_4 = temp_v0_2->field_88;
                            if (temp_v1_4 < 0x1000) {
                                temp_v0_2->field_88 = (s32)(temp_v1_4 + 0x200);
                            }
                        } else {
                            temp_v0_5 = temp_v0_2->field_88;
                            if (temp_v0_5 > 0) {
                                temp_v0_2->field_88 = (s32)(temp_v0_5 - 0x80);
                            }
                        }
                        temp_v0_2->field_8C = (s16)(u16)temp_v0_2->field_8E;
                        if (temp_v0_2->field_88 != 0) {
                            temp_v1_5       = &temp_v0_2->field_0[temp_a0_3];
                            temp_v1_5[0]    = (s16)(u16)temp_v0_2->field_84;
                            temp_v1_5[0x20] = (u16)temp_v0_2->field_88;
                            if (temp_v0_2->field_E0 == 0) {
                                var_v0_2 = temp_v0_2->field_84 + 0x180;
                            } else {
                                var_v0_2 = temp_v0_2->field_84 + 0x100;
                            }
                            temp_v0_2->field_84 = var_v0_2;
                        }
                        var_a1 += 1;
                    } while (var_a1 < 0x10);
                    break;
            }
            arg0->state = (s32)(arg0->state + 1);
        } else {
            Task_CallExit(arg0);
            return;
        }
    }
    temp_s0 = (ActorEffectState*)arg0->work;
    if (Gp_StateF0.field_4 == 0) {
        temp_v1_9 = arg0->spawnArg1;
        switch (temp_v1_9) { /* switch 1; irregular */
            case 1:          /* switch 1 */
                temp_v0_9         = temp_s0->field_E4 - 1;
                temp_s0->field_E4 = temp_v0_9;
                if (temp_v0_9 == 0) {
                    temp_a0_5               = ((Task*)arg0->spawnArg2)->extra;
                    D_actor_403600_801606A0 = NULL;
                    temp_a0_5->flags        = (u16)(temp_a0_5->flags | 0x80);
                    goto block_57;
                } else if (temp_v0_9 > 0) {
                    D_actor_403600_801606A0 = (s32)&temp_s0->field_90;
                    Gp_UpdateCoord(&temp_s0->field_90);
                    goto block_57;
                }
                goto block_57;
            case 2: /* switch 1 */
                temp_v1_10        = temp_s0->field_E4 - 1;
                temp_s0->field_E4 = temp_v1_10;
                if (temp_v1_10 != 0) {
                    goto block_52;
                }
                temp_a1                 = ((Task*)arg0->spawnArg2)->extra;
                temp_a0_6               = (s16*)&temp_s0->field_90;
                D_actor_403600_801606A0 = temp_a0_6;
                temp_a1->flags          = (u16)(temp_a1->flags & 0xFF7F);
                Gp_UpdateCoord((GsCOORDINATE2*)temp_a0_6);
                goto block_57;
            block_52:
                if (temp_v1_10 < -7) {
                    goto block_55;
                }
                D_actor_403600_801606A0 = (s32)&temp_s0->field_90;
                Gp_UpdateCoord(&temp_s0->field_90);
                goto block_57;
            block_55:
                if (temp_v1_10 == -8) {
                    D_actor_403600_801606A0 = NULL;
                }
            block_57:
                break;
        }
        if (arg0->killCountdown > 0) {
            temp_s0->field_8E = 1;
        } else {
            temp_s0->field_8E = 0;
        }
        arg0->killCountdown = (s16)((u16)arg0->killCountdown - 1);
        temp_a0_7           = temp_s0->field_80;
        temp_v1_11          = temp_a0_7 + 0x1F;
        var_v0_5            = temp_v1_11;
        temp_s0->field_80   = temp_v1_11;
        if (temp_v1_11 < 0) {
            var_v0_5 = temp_a0_7 + 0x3E;
        }
        temp_v0_10 = temp_v1_11 - ((var_v0_5 >> 5) << 5);
        __asm__("move %0,%1" : "=r"(temp_a0_7) : "r"(temp_v0_10));
        temp_s0->field_80 = temp_v0_10;
        temp_v0_11        = &temp_s0->field_0[temp_a0_7];
        temp_v0_11[0]     = 0;
        temp_v0_11[0x20]  = 0U;
        if (temp_s0->field_8E != 0) {
            if (temp_s0->field_8C == 0) {
                temp_s0->field_84 = 0;
            }
            temp_v1_12 = temp_s0->field_88;
            if (temp_v1_12 < 0x1000) {
                temp_s0->field_88 = (s32)(temp_v1_12 + 0x200);
            }
        } else {
            temp_v0_12 = temp_s0->field_88;
            if (temp_v0_12 > 0) {
                temp_s0->field_88 = (s32)(temp_v0_12 - 0x80);
            }
        }
        temp_s0->field_8C = (s16)(u16)temp_s0->field_8E;
        if (temp_s0->field_88 != 0) {
            temp_v0_13       = &temp_s0->field_0[temp_a0_7];
            temp_v0_13[0]    = (s16)(u16)temp_s0->field_84;
            temp_v0_13[0x20] = (u16)temp_s0->field_88;
            if (temp_s0->field_E0 != 0) {
                goto block_73;
            }
            var_v0_6 = temp_s0->field_84 + 0x180;
            goto block_74;
        block_72:
            var_v0_7 = 0;
            goto block_78;
        block_73:
            var_v0_6 = temp_s0->field_84 + 0x100;
        block_74:
            temp_s0->field_84 = var_v0_6;
        }
    }
    func_actor_403600_801353D0(temp_s0, temp_s4);
    var_v1 = 0;
    if (arg0->killCountdown <= 0) {
        var_a0_2 = temp_s0->field_0;
    loop_78:
        var_v1 += 1;
        if (*var_a0_2 != 0) {
            goto block_72;
        }
        var_a0_2 += 1;
        if (var_v1 >= 0x20) {
            var_v0_7 = 1;
        } else {
            goto loop_78;
        }
    block_78:
        if (var_v0_7 != 0) {
            Task_CallExit(arg0);
        }
    }
}

u32* func_actor_403600_80136224(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc = upper_y - 0x168;
                            SOFT_TOUCH_REG(upper_calc);
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        *(u32*)&poly->r0 = 0;
                        *(u32*)&poly->r1 = 0;
                        *(u32*)&poly->r2 = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (upper_limit < lower_y) {
                            lower_delta  = lower_y;
                            lower_delta -= 0x168;
                            lower_delta += light;
                            lower_delta *= 2;
                            poly->y1    -= lower_delta;
                            poly->y2    -= lower_delta;
                            poly->y0    -= lower_delta;
                        }
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x36);
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80136500(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           mask_hi;
    u32           clip_mask;
    u16*          rec;
    s32           light;
    s32           upper_limit;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           sz;
    s32           idx;
    s32           first;
    u8*           sz_table;
    s16           upper_y;
    s16           lower_y;

    poly  = (POLY_GT3*)arg0->preXformWrite;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        mask_hi     = 0xFF000000;
        do {
            rec = (u16*)arg2;
            actor_403600_ldsxy3_fifo_gt3((u8*)poly + 7);
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult > 0) {
                sz_table = (u8*)arg0->szTable;
                idx      = rec[0] & 0xFFFC;
                sz       = *(s32*)(idx + (s32)sz_table);
                if (!(sz & clip_mask)) {
                    actor_403600_ldsz1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)sz_table);
                    if (!(sz & clip_mask)) {
                        actor_403600_ldsz2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)sz_table);
                        if (!(sz & clip_mask)) {
                            actor_403600_ldsz3(sz);
                            gte_avsz3();
                            upper_delta = 0;
                            if (light != 0) {
                                upper_y = poly->y0;
                                if (upper_limit < upper_y) {
                                    upper_calc = upper_y - 0x168;
                                    SOFT_TOUCH_REG(upper_calc);
                                    upper_delta = (upper_calc + light) * 2;
                                }
                                if (upper_delta >= 0x81) {
                                    *(u32*)&poly->r0 = 0;
                                    *(u32*)&poly->r1 = 0;
                                    *(u32*)&poly->r2 = 0;
                                } else {
                                    first       = (u8)poly->r0;
                                    upper_delta = 0x80 - upper_delta;
                                    actor_403600_fade_rgb((u8*)poly + 7, upper_delta, first);
                                }
                            }
                            if (light != 0) {
                                lower_y = poly->y0;
                                if (upper_limit < lower_y) {
                                    lower_delta  = lower_y;
                                    lower_delta -= 0x168;
                                    lower_delta += light;
                                    lower_delta *= 2;
                                    poly->y1    -= lower_delta;
                                    poly->y2    -= lower_delta;
                                    poly->y0    -= lower_delta;
                                }
                            }
                            setlen(poly, 9);
                            setcode(poly, 0x36);
                            gte_stotz(opz);
                            poly->tag = (poly->tag & mask_hi) |
                                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                    (s32)arg0->ot) &
                                         mask);
                            *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                       (s32)arg0->ot) =
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                            (s32)arg0->ot) &
                                 mask_hi) |
                                ((u32)poly & mask);
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->preXformWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_8013685C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT4*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    s32*          flg;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT4*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    gte_ldrgb(&col);
    if (arg0->elemCount-- > 0) {
        flg         = &arg0->gteFlag;
        clip_mask   = 0x80000000;
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if (!(arg0->gteFlag & clip_mask)) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if (!(arg0->gteFlag & clip_mask)) {
                    if (arg0->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (arg0->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        upper_delta = 0;
                        if (light != 0) {
                            upper_y = poly->y0;
                            if (upper_limit < upper_y) {
                                upper_calc  = upper_y - 0x168;
                                upper_delta = (upper_calc + light) * 2;
                            }
                        }
                        if (upper_delta >= 0x81) {
                            *(u32*)&poly->r0 = 0;
                            *(u32*)&poly->r1 = 0;
                            *(u32*)&poly->r2 = 0;
                            *(u32*)&poly->r3 = 0;
                        } else {
                            col.r = -0x80 - upper_delta;
                            col.g = -0x80 - upper_delta;
                            col.b = -0x80 - upper_delta;
                            gte_ldrgb(&col);
                            norms = (u8*)arg0->normals;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                        }
                        if (light != 0) {
                            lower_y = poly->y0;
                            if (upper_limit < lower_y) {
                                lower_delta  = lower_y;
                                lower_delta -= 0x168;
                                lower_delta += light;
                                lower_delta *= 2;
                                poly->y3    -= lower_delta;
                                poly->y2    -= lower_delta;
                                poly->y1    -= lower_delta;
                                poly->y0    -= lower_delta;
                            }
                        }
                        setlen(poly, 12);
                        setcode(poly, 0x3E);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & 0xFF000000) |
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask);
                        *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                   (s32)arg0->ot) =
                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                        (s32)arg0->ot) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80136C00(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           mask_hi;
    u32           clip_mask;
    u16*          rec;
    s32           light;
    s32           upper_limit;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           sz;
    s32           idx;
    s32           first;
    u8*           sz_table;
    s16           upper_y;
    s16           lower_y;

    poly  = (POLY_GT4*)arg0->preXformWrite;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        mask_hi = 0xFF000000;
        do {
            rec = (u16*)arg2;
            actor_403600_ldsxy3_fifo_gt4((u8*)poly + 7);
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult > 0) {
                goto draw;
            }
            actor_403600_ldsxy1_fifo_gt4((u8*)poly + 7);
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult < 0) {
            draw:
                sz_table = (u8*)arg0->szTable;
                idx      = rec[0] & 0xFFFC;
                sz       = *(s32*)(idx + (s32)sz_table);
                if (!(sz & clip_mask)) {
                    actor_403600_ldsz0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)sz_table);
                    if (!(sz & clip_mask)) {
                        actor_403600_ldsz1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)sz_table);
                        if (!(sz & clip_mask)) {
                            actor_403600_ldsz2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = *(s32*)(idx + (s32)sz_table);
                            if (!(sz & clip_mask)) {
                                actor_403600_ldsz3(sz);
                                gte_avsz4();
                                upper_delta = 0;
                                if (light != 0) {
                                    upper_y = poly->y0;
                                    if (upper_limit < upper_y) {
                                        upper_calc  = upper_y - 0x168;
                                        upper_delta = (upper_calc + light) * 2;
                                    }
                                    if (upper_delta >= 0x81) {
                                        *(u32*)&poly->r0 = 0;
                                        *(u32*)&poly->r1 = 0;
                                        *(u32*)&poly->r2 = 0;
                                        *(u32*)&poly->r3 = 0;
                                    } else {
                                        first       = (u8)poly->r0;
                                        upper_delta = 0x80 - upper_delta;
                                        actor_403600_fade_rgb_staged((u8*)poly + 7, upper_delta, first);
                                    }
                                }
                                if (light != 0) {
                                    lower_y = poly->y0;
                                    if (upper_limit < lower_y) {
                                        lower_delta  = lower_y;
                                        lower_delta -= 0x168;
                                        lower_delta += light;
                                        lower_delta *= 2;
                                        poly->y3    -= lower_delta;
                                        poly->y2    -= lower_delta;
                                        poly->y1    -= lower_delta;
                                        poly->y0    -= lower_delta;
                                    }
                                }
                                setlen(poly, 12);
                                setcode(poly, 0x3E);
                                gte_stotz(opz);
                                gte_stotz(opz);
                                poly->tag = (poly->tag & mask_hi) |
                                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                        (s32)arg0->ot) &
                                             mask);
                                *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                           (s32)arg0->ot) =
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask_hi) |
                                    ((u32)poly & mask);
                            }
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->preXformWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_8013700C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc = upper_y - 0x168;
                            SOFT_TOUCH_REG(upper_calc);
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        *(u32*)&poly->r0 = 0;
                        *(u32*)&poly->r1 = 0;
                        *(u32*)&poly->r2 = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (lower_y < (light - 0x168)) {
                            lower_delta  = lower_y;
                            lower_delta += 0x168;
                            lower_delta -= light;
                            lower_delta *= 2;
                            poly->y1    += lower_delta;
                            poly->y2    += lower_delta;
                            poly->y0    += lower_delta;
                            poly->code  |= 2;
                        }
                    }
                    poly->code &= 0xFE;
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80137300(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc = upper_y - 0x168;
                            SOFT_TOUCH_REG(upper_calc);
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        *(u32*)&poly->r0 = 0;
                        *(u32*)&poly->r1 = 0;
                        *(u32*)&poly->r2 = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (lower_y < (light - 0x168)) {
                            lower_delta  = lower_y;
                            lower_delta += 0x168;
                            lower_delta -= light;
                            lower_delta *= 2;
                            poly->y1    += lower_delta;
                            poly->y2    += lower_delta;
                            poly->y0    += lower_delta;
                            poly->code  |= 2;
                        }
                    }
                    poly->code = (poly->code & 0xFE) | 2;
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_801375F8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT4*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    s32*          flg;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT4*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    gte_ldrgb(&col);
    if (arg0->elemCount-- > 0) {
        flg         = &arg0->gteFlag;
        clip_mask   = 0x80000000;
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if (!(arg0->gteFlag & clip_mask)) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if (!(arg0->gteFlag & clip_mask)) {
                    if (arg0->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (arg0->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        upper_delta = 0;
                        if (light != 0) {
                            upper_y = poly->y0;
                            if (upper_limit < upper_y) {
                                upper_calc  = upper_y - 0x168;
                                upper_delta = (upper_calc + light) * 2;
                            }
                        }
                        if (upper_delta >= 0x81) {
                            *(u32*)&poly->r0 = 0;
                            *(u32*)&poly->r1 = 0;
                            *(u32*)&poly->r2 = 0;
                            *(u32*)&poly->r3 = 0;
                        } else {
                            col.r = -0x80 - upper_delta;
                            col.g = -0x80 - upper_delta;
                            col.b = -0x80 - upper_delta;
                            gte_ldrgb(&col);
                            norms = (u8*)arg0->normals;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                        }
                        setlen(poly, 12);
                        setcode(poly, 0x3C);
                        if (light != 0) {
                            lower_y = poly->y0;
                            if (lower_y < (light - 0x168)) {
                                lower_delta  = lower_y;
                                lower_delta += 0x168;
                                lower_delta -= light;
                                lower_delta *= 2;
                                poly->y3    += lower_delta;
                                poly->y2    += lower_delta;
                                poly->y1    += lower_delta;
                                poly->y0    += lower_delta;
                                poly->code  |= 2;
                            }
                        }
                        poly->code &= 0xFE;
                        gte_stotz(opz);
                        poly->tag = (poly->tag & 0xFF000000) |
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask);
                        *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                   (s32)arg0->ot) =
                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                        (s32)arg0->ot) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_801379B4(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR          color;
    SVECTOR          local;
    u8*              head;
    u8*              scratch;
    MATRIX*          saved;
    register MATRIX* transposed asm("s0");
    MATRIX*          active;
    MATRIX*          projected;
    u32*             stream;
    u16*             record;
    u8*              verts;
    u8*              norms;
    u8*              coord;
    u8*              local_stack;
    u8*              local_addr;
    POLY_GT3*        poly;
    register s32*    opz asm("s1");
    register u32     mask asm("t2");
    u32              mask_hi;
    u32              ds_high;
    DisplayState*    ds;
    s32              i;
    s32              offset;
    register u8*     index asm("a0");
    register u8*     clamp asm("a1");

    __asm__("move %0,%1" : "=r"(stream) : "r"(arg2), "r"(D_actor_403600_801606A0));
    if (D_actor_403600_801606A0 != NULL) {
        poly    = (POLY_GT3*)arg0->primWrite;
        color   = D_actor_403600_80131E34;
        head    = SCRATCH_HEAD(u8);
        scratch = (SCRATCH_HEAD(u8) = head - 0x7C);
        gte_sttr(scratch);
        saved = (MATRIX*)(head - 0x40);
        gte_ReadRotMatrix(saved);
        transposed = (MATRIX*)(head - 0x20);
        TransposeMatrix((MATRIX*)(D_actor_403600_801606A0 + 0x24), transposed);

        coord = (u8*)D_actor_403600_801606A0;
        SOFT_BARRIER();
        *(s16*)(scratch + 0x10) = *(u16*)(head - 0x7C) - *(u16*)(coord + 0x38);
        *(s16*)(scratch + 0x12) = *(u16*)(scratch + 0x04) - *(u16*)(coord + 0x3C);
        *(s16*)(scratch + 0x14) = *(u16*)(scratch + 0x08) - *(u16*)(coord + 0x40);

        SOFT_BARRIER();
        local_stack = (u8*)&local;
        local_addr  = head - 0x6C;
        local       = *(SVECTOR*)local_addr;
        gte_SetRotMatrix(transposed);
        gte_ldv0(local_stack);
        gte_rtv0();
        gte_stsv(local_addr);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x70) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x74) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x78) = *(s16*)(scratch + 0x14);
        gte_ldrgb(&color);
        if (arg0->elemCount-- > 0) {
            __asm__("move %0,%1" : "=r"(active) : "r"(transposed));
            projected = (MATRIX*)(head - 0x64);
            SOFT_TOUCH_REG(projected);
            opz = &arg0->gteResult;
            __asm__("lui %0,%%hi(gDisplayState)" : "=r"(ds_high));
            __asm__("addiu %0,%1,%%lo(gDisplayState)" : "=&r"(ds) : "r"(ds_high));
            SOFT_TOUCH_REG(ds);
            mask    = 0xFFFFFF;
            mask_hi = 0xFF000000;
            do {
                record = (u16*)stream;
                gte_SetTransMatrix(active);
                gte_SetRotMatrix(active);
                i                       = 0;
                clamp                   = scratch;
                *(s32*)(scratch + 0x30) = record[0] >> 3;
                offset                  = 0x18;
                *(s32*)(scratch + 0x34) = record[1] >> 3;
                *(s32*)(scratch + 0x38) = record[2] >> 3;
                __asm__("move %0,%1" : "=r"(index) : "r"(scratch));
                do {
                    verts = (u8*)arg0->verts;
                    gte_ldv0(verts + (*(s32*)(index + 0x30) << 3));
                    gte_rtv0tr();
                    gte_stsv(scratch + offset);
                    if (*(s16*)(clamp + 0x1A) > 0) {
                        *(s16*)(clamp + 0x1A) = 0;
                    }
                    SOFT_TOUCH_REG(clamp);
                    clamp  += 8;
                    offset += 8;
                    i++;
                    index += 4;
                } while (i < 3);

                gte_SetRotMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                gte_SetTransMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                gte_ldv3(projected, scratch + 0x20, scratch + 0x28);
                gte_rtpt();
                gte_stflg(&arg0->gteFlag);
                if (arg0->gteFlag >= 0) {
                    gte_nclip();
                    gte_stopz(opz);
                    if (arg0->gteResult > 0) {
                        gte_stsxy3_gt3(poly);
                        gte_avsz3();
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (record[3] & 0xFFF8),
                                 norms + (record[4] & 0xFFF8),
                                 norms + (record[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                        setlen(poly, 9);
                        setcode(poly, 0x34);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & mask_hi) |
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask);
                        *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                   (s32)arg0->ot) =
                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                        (s32)arg0->ot) &
                             mask_hi) |
                            ((u32)poly & mask);
                    }
                }
                poly++;
                stream += arg0->elemStride;
            } while (arg0->elemCount-- > 0);
        }
        arg0->primWrite = (u8*)poly;
        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x3C);
        SCRATCH_POP_BYTES(0x7C);
        return stream;
    }
    return tmdDrawStreamGt3(arg0, arg1, stream);
}

u32* func_actor_403600_80138004(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR          color;
    SVECTOR          local;
    u8*              head;
    u8*              scratch;
    register MATRIX* saved asm("s1");
    register MATRIX* transposed asm("s0");
    MATRIX*          active;
    u32*             stream;
    u16*             record;
    u8*              verts;
    u8*              norms;
    u8*              coord;
    u8*              local_stack;
    u8*              local_addr;
    register u8*     color_ds asm("s2");
    POLY_GT4*        poly;
    s32*             flg;
    register s32*    opz asm("t3");
    u32              clip_mask;
    u32              mask;
    u32              ds_high;
    s32              i;
    s32              offset;
    register u8*     index asm("a0");
    register u8*     clamp asm("a1");

    __asm__("move %0,%1" : "=r"(stream) : "r"(arg2), "r"(D_actor_403600_801606A0));
    if (D_actor_403600_801606A0 != NULL) {
        poly    = (POLY_GT4*)arg0->primWrite;
        color   = D_actor_403600_80131E34;
        head    = SCRATCH_HEAD(u8);
        scratch = (SCRATCH_HEAD(u8) = head - 0x88);
        gte_sttr(scratch);
        saved = (MATRIX*)(head - 0x40);
        gte_ReadRotMatrix(saved);
        transposed = (MATRIX*)(head - 0x20);
        TransposeMatrix((MATRIX*)(D_actor_403600_801606A0 + 0x24), transposed);

        coord = (u8*)D_actor_403600_801606A0;
        SOFT_BARRIER();
        *(s16*)(scratch + 0x10) = *(u16*)(head - 0x88) - *(u16*)(coord + 0x38);
        *(s16*)(scratch + 0x12) = *(u16*)(scratch + 0x04) - *(u16*)(coord + 0x3C);
        *(s16*)(scratch + 0x14) = *(u16*)(scratch + 0x08) - *(u16*)(coord + 0x40);

        SOFT_BARRIER();
        local_stack = (u8*)&local;
        local_addr  = head - 0x78;
        local       = *(SVECTOR*)local_addr;
        gte_SetRotMatrix(transposed);
        gte_ldv0(local_stack);
        gte_rtv0();
        gte_stsv(local_addr);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x7C) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x80) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x84) = *(s16*)(scratch + 0x14);
        color_ds                = (u8*)&color;
        gte_ldrgb(color_ds);
        if (arg0->elemCount-- > 0) {
            active = transposed;
            SOFT_USE_REG(head);
            flg       = &arg0->gteFlag;
            clip_mask = 0x80000000;
            opz       = &arg0->gteResult;
            __asm__("lui %0,%%hi(gDisplayState)" : "=r"(ds_high));
            __asm__("addiu %0,%1,%%lo(gDisplayState)" : "=&r"(color_ds) : "r"(ds_high));
            mask = 0xFFFFFF;
            SOFT_TOUCH_REG(mask);
            saved = (MATRIX*)0xFF000000;
            do {
                record = (u16*)stream;
                gte_SetTransMatrix(active);
                gte_SetRotMatrix(active);
                i                       = 0;
                clamp                   = scratch;
                *(s32*)(scratch + 0x38) = record[0] >> 3;
                offset                  = 0x18;
                *(s32*)(scratch + 0x3C) = record[1] >> 3;
                *(s32*)(scratch + 0x40) = record[2] >> 3;
                *(s32*)(scratch + 0x44) = record[3] >> 3;
                __asm__("move %0,%1" : "=r"(index) : "r"(scratch));
                do {
                    verts = (u8*)arg0->verts;
                    gte_ldv0(verts + (*(s32*)(index + 0x38) << 3));
                    gte_rtv0tr();
                    gte_stsv(scratch + offset);
                    if (*(s16*)(clamp + 0x1A) > 0) {
                        *(s16*)(clamp + 0x1A) = 0;
                    }
                    SOFT_TOUCH_REG(clamp);
                    clamp  += 8;
                    offset += 8;
                    i++;
                    index += 4;
                } while (i < 4);

                gte_SetRotMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                gte_SetTransMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                index = scratch + 0x18;
                gte_ldv3(index, scratch + 0x20, scratch + 0x28);
                gte_rtpt();
                gte_stflg(flg);
                if (!(arg0->gteFlag & clip_mask)) {
                    gte_nclip();
                    gte_stopz(opz);
                    gte_stsxy3_gt4(poly);
                    gte_ldv0(scratch + 0x30);
                    gte_rtps();
                    gte_stflg(flg);
                    if (!(arg0->gteFlag & clip_mask)) {
                        if (arg0->gteResult > 0) {
                            goto draw;
                        }
                        gte_nclip();
                        gte_stopz(opz);
                        if (arg0->gteResult < 0) {
                        draw:
                            gte_stsxy2(&poly->x3);
                            gte_avsz4();
                            norms = (u8*)arg0->normals;
                            gte_ldv3(norms + (record[4] & 0xFFF8),
                                     norms + (record[5] & 0xFFF8),
                                     norms + (record[6] & 0xFFF8));
                            gte_ncct();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->normals + (record[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                            setlen(poly, 12);
                            setcode(poly, 0x3C);
                            gte_stotz(opz);
                            poly->tag = (poly->tag & (u32)saved) |
                                        (*(u_long*)(((((u32)arg0->gteResult << ((DisplayState*)color_ds)->otDepthShift) >> 2) &
                                                     0xFFC) +
                                                    (s32)arg0->ot) &
                                         mask);
                            *(u_long*)(((((u32)arg0->gteResult << ((DisplayState*)color_ds)->otDepthShift) >> 2) & 0xFFC) +
                                       (s32)arg0->ot) =
                                (*(u_long*)(((((u32)arg0->gteResult << ((DisplayState*)color_ds)->otDepthShift) >> 2) & 0xFFC) +
                                            (s32)arg0->ot) &
                                 (u32)saved) |
                                ((u32)poly & mask);
                        }
                    }
                }
                poly++;
                stream += arg0->elemStride;
            } while (arg0->elemCount-- > 0);
        }
        arg0->primWrite = (u8*)poly;
        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x48);
        SCRATCH_POP_BYTES(0x88);
        return stream;
    }
    return tmdDrawStreamGt4(arg0, arg1, stream);
}

u32* func_actor_403600_801386EC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR color;
    SVECTOR local;
    u8*     head;
    u8*     scratch;
    MATRIX* saved;
    MATRIX* transposed;
    MATRIX* active;
    u8*     stream;
    u8*     record;
    u8*     coord;
    s32     previous;
    u16     colorOffset;
    s32     count;
    s32     loadedCount;

    __asm__("move %0,%1" : "=r"(stream) : "r"(arg2), "r"(D_actor_403600_801606A0));
    if (D_actor_403600_801606A0 != NULL) {
        previous = -1;
        color    = D_actor_403600_80131E34;
        if (arg0->elemCount == 0) {
            return (u32*)stream;
        }

        head    = SCRATCH_HEAD(u8);
        scratch = (SCRATCH_HEAD(u8) = head - 0x7C);
        gte_sttr(scratch);
        saved = (MATRIX*)(head - 0x40);
        TOUCH_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        SOFT_USE_REG(saved);
        gte_ReadRotMatrix(saved);
        transposed = (MATRIX*)(head - 0x20);
        TransposeMatrix((MATRIX*)(D_actor_403600_801606A0 + 0x24), transposed);

        coord = (u8*)D_actor_403600_801606A0;
        SOFT_BARRIER();
        *(s16*)(scratch + 0x10) = *(u16*)(head - 0x7C) - *(u16*)(coord + 0x38);
        *(s16*)(scratch + 0x12) = *(u16*)(scratch + 0x04) - *(u16*)(coord + 0x3C);
        *(s16*)(scratch + 0x14) = *(u16*)(scratch + 0x08) - *(u16*)(coord + 0x40);

        local = *(SVECTOR*)(head - 0x6C);
        gte_SetRotMatrix(transposed);
        gte_ldv0(&local);
        gte_rtv0();
        gte_stsv(scratch + 0x10);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x70) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x74) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x78) = *(s16*)(scratch + 0x14);
        gte_ldrgb(&color);

        if (arg0->elemCount-- > 0) {
            active = transposed;
            saved  = (MATRIX*)(head - 0x64);
            do {
                record = stream;
                if (*(u16*)(stream + 0) != previous) {
                    gte_SetTransMatrix(active);
                    gte_SetRotMatrix(active);
                    gte_ldv0((u8*)arg0->verts + (*(u16*)(stream + 0) & 0xFFF8));
                    gte_rtv0tr();
                    gte_stsv(saved);
                    if (*(s16*)(scratch + 0x1A) > 0) {
                        *(s16*)(scratch + 0x1A) = 0;
                    }
                    gte_SetRotMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                    gte_SetTransMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                    gte_ldv0(saved);
                    gte_rtps();
                    gte_stsz(&arg0->gteResult);
                    gte_stflg(&arg0->gteFlag);
                    if (arg0->gteFlag & 0x80000000) {
                        arg0->gteResult |= 0x80000000;
                    }
                    arg0->szTable[*(u16*)(stream + 0) >> 3] = arg0->gteResult;
                }
                gte_stsxy(arg0->preXformWrite + *(u16*)(record + 4));
                gte_ldv0((u8*)arg0->normals + (*(u16*)(record + 2) & 0xFFF8));
                gte_nccs();
                colorOffset = *(u16*)(record + 6);
                stream     += arg0->elemStride * 4;
                gte_strgb(arg0->preXformWrite + colorOffset);
                loadedCount = arg0->elemCount;
                SOFT_USE_REG(loadedCount);
                previous = *(u16*)(record + 0);
                __asm__ volatile("move %0,%1" : "=r"(count) : "r"(loadedCount));
                arg0->elemCount = loadedCount - 1;
            } while (count > 0);
        }

        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x3C);
        SCRATCH_POP_BYTES(0x7C);
        return (u32*)stream;
    }
    return tmdXformStreamVerts(arg0, arg1, (u32*)stream);
}

const CVECTOR D_actor_403600_80131E34 = { 0x80, 0x80, 0x80, 0 };
