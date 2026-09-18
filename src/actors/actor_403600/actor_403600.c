#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtir_real()   __asm__ volatile("nop; nop; .word 0x4A49E012")
#define gte_rtpt_real()   __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_nclip_real()  __asm__ volatile("nop; nop; .word 0x4B400006")
#define gte_avsz3_real()  __asm__ volatile("nop; nop; .word 0x4B58002D")
#define gte_avsz4_real()  __asm__ volatile("nop; nop; .word 0x4B68002E")
#define gte_ncct_real()   __asm__ volatile("nop; nop; .word 0x4B18043F")
#define gte_mvmva_10030() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_mvmva_10000() __asm__ volatile("nop; nop; .word 0x4A480012")
#define gte_nccs_real()   __asm__ volatile("nop; nop; .word 0x4B08041B")
#define actor_403600_load_scratch_head(out) \
    __asm__ volatile("lui %0, 0x1F80; lw %0, 0x3FC(%0)" : "=r"(out))
#define actor_403600_load_scratch_head_nop(out) \
    __asm__ volatile("lui %0, 0x1F80; lw %0, 0x3FC(%0); nop" : "=r"(out))
#define actor_403600_store_scratch_head(value) \
    __asm__ volatile("lui $1, 0x1F80; sw %0, 0x3FC($1)" : : "r"(value) : "$1", "memory")
#define actor_403600_restore_scratch_head(value) \
    __asm__("addiu %0, %0, 24; lui $1, 0x1F80; sw %0, 0x3FC($1)" : "+r"(value) : : "$1")
#define actor_403600_color_tail(z, a3, block, work, zero)                                   \
    __asm__ volatile("lw %0, 1272(%3); addu %1, %4, $zero; lui $1, 0x1F80; sw %2, 1020($1)" \
                     : "=r"(z), "=r"(a3), "+r"(block) : "r"(work), "r"(zero) : "$1")
#define actor_403600_color_tail_in_place(work, a3, block, zero)                             \
    __asm__ volatile("lw %0, 1272(%0); addu %1, %3, $zero; lui $1, 0x1F80; sw %2, 1020($1)" \
                     : "+r"(work), "=r"(a3), "+r"(block) : "r"(zero) : "$1")
#define actor_403600_rot_setup(object, rot, matrix, coord, actor, work, head)             \
    __asm__ volatile("lw %0, 44(%4); addiu %1, %5, 1792; addiu %2, %6, -32; lw %3, 8(%0)" \
                     : "=r"(object), "=r"(rot), "=r"(matrix), "=r"(coord)                 \
                     : "r"(actor), "r"(work), "r"(head))
#define actor_403600_coord_advance(head, work) \
    __asm__ volatile("sw $zero, 1208(%1); addiu %0, %0, 32" : "+r"(head) : "r"(work))
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

extern u8               D_80071075;
extern s8               D_8007216C;
extern u8               D_801153F4;
extern MATRIX*          D_80073B8C;
extern s32              D_8005C374;
extern s32              D_8007107C;
extern s16              D_80073BA0;
extern u32              Gp_LcgState;
extern u8               D_actor_403600_80150ED4;
extern TaskDesc         D_actor_403600_801421A0;
extern s32              D_actor_403600_8016056C;
extern Actor403600Msg   D_actor_403600_80160568;
extern s32              D_actor_403600_8016057C[];
extern Actor403600Point D_actor_403600_801605F4[];
extern s32              D_actor_403600_80160698;
extern s32              D_actor_403600_8016069C;
extern s32              D_actor_403600_801606A0;
extern GpU16Pair        D_actor_403600_801606A4;
extern Task*            D_actor_403600_801606A8;
extern GpU16Pair        D_actor_403600_80150EB0;
extern const SVECTOR    D_actor_403600_80131E2C;
extern CVECTOR          D_actor_403600_80131E34;
extern GpPairSrcE       D_actor_403600_80150EC8;
extern GpU16Pair        D_actor_403600_80150E9C;
extern s32              D_actor_403600_80160504[4];
extern Actor403600Pair  D_actor_403600_801606B8;
extern Task*            D_actor_403600_801606AC;
extern s32              D_actor_403600_801606BC;

// Typed accesses change GCC 2.8.1's alias/CSE decisions in this initializer.
#define ACTOR_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8*)(expr) + (offset)))

void func_actor_403600_80141598(Task* arg0);
void func_actor_403600_8014174C(Task* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_403600_801353D0(Actor403600EffectState* arg0, GsCOORDINATE2* arg1);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_actor_403600_80132A18(Task* arg0, Actor403600Work* arg1, TaskIdMap* arg2, TaskIdMap* arg3);
void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600Work* arg2);
void func_actor_403600_8013955C(Actor403600* arg0);
void func_actor_403600_801396F8(Actor403600* arg0);
void func_actor_403600_8013D15C(Actor403600* arg0);
void func_actor_403600_8013DC7C(Actor403600* arg0);
void func_actor_403600_8013F0C0(Actor403600* arg0);
void func_actor_403600_801411D4(Actor403600* arg0, s32 arg1);
void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1);
void func_actor_403600_80141338(Actor403600* arg0);
void func_actor_403600_801414FC(Actor403600* arg0);
void func_actor_403600_8014161C(Actor403600* arg0);
void func_actor_403600_80141954(s32 arg0);
void func_actor_403600_80141A34(Actor403600* arg0);
void func_actor_403600_80141B24(Actor403600* arg0);
void func_actor_403600_80141C3C(Actor403600* arg0);
s32  func_actor_403600_801320F8(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801320F8);

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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80132A18);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80132E40);

void func_actor_403600_80134288(Task* arg0)
{
    Task*                     temp_v0_2;
    TaskIdMap*                temp_a3;
    register TaskIdMap*       temp_v0 asm("a3");
    register Actor403600Work* var_a2 asm("a2");

    var_a2 = (Actor403600Work*)arg0->parent->work;
    if (arg0->state == 0) {
        temp_v0 = Mem_Calloc(0x11C, false);
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
    D_actor_403600_8016069C = D_8005C374 + (D_8007107C * 0xC000);
    {
        register s32 field_742 asm("v1");

        field_742 = var_a2->field_742;
        if ((field_742 != 1) && (var_a2->field_708 > 0)) {
            func_actor_403600_80132A18(arg0, var_a2, temp_a3, temp_a3);
        }
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80134398);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801353D0);

const SVECTOR D_actor_403600_80131E2C = { 0, 0x578, 0, 0 };

void func_actor_403600_80135C28(Task* arg0)
{
    SVECTOR                 sp10;
    s16*                    temp_a0_2;
    s16*                    temp_a0_6;
    Actor403600EffectState* temp_s0;
    s16*                    temp_v0_11;
    s16*                    temp_v0_13;
    Actor403600EffectState* temp_v0_2;
    s16*                    temp_v0_4;
    s16*                    temp_v0_7;
    s16*                    temp_v1_5;
    s16*                    temp_v1_8;
    s16*                    var_a0_2;
    GsCOORDINATE2*          temp_s4;
    s32                     temp_a0_3;
    s32                     temp_a0_4;
    s32                     temp_a0_7;
    s32                     temp_v0_10;
    s32                     temp_v0_12;
    s32                     temp_v0_3;
    s32                     temp_v0_5;
    s32                     temp_v0_6;
    s32                     temp_v0_8;
    s32                     temp_v0_9;
    s32                     temp_v1_10;
    s32                     temp_v1_11;
    s32                     temp_v1_12;
    s32                     temp_v1_2;
    s32                     temp_v1_3;
    s32                     temp_v1_4;
    s32                     temp_v1_6;
    s32                     temp_v1_7;
    s32                     temp_v1_9;
    s32                     var_a1;
    s32                     var_a1_2;
    s32                     var_v0;
    s32                     var_v0_2;
    s32                     var_v0_3;
    s32                     var_v0_4;
    s32                     var_v0_5;
    s32                     var_v0_6;
    s32                     var_v0_7;
    s32                     var_v1;
    Actor403600*            temp_a0;
    TmdObject*              temp_a0_5;
    TmdObject*              temp_a1;
    Task*                   temp_s2;
    TmdObject*              temp_v0;
    Actor403600Work*        temp_v1;

    temp_a0 = arg0->spawnArg2;
    temp_v1 = temp_a0->field_1C;
    temp_s4 = ((TmdObject*)arg0->extra)->field_8;
    temp_s2 = temp_v1->field_710;
    if (temp_v1->field_742 == 1) {
        temp_v0                 = temp_a0->field_2C;
        D_actor_403600_801606A0 = NULL;
        temp_v0->field_C        = (u16)(temp_v0->field_C & 0xFF7F);
        Task_CallExit(arg0);
        return;
    }
    if (arg0->state == 0) {
        temp_v0_2 = Mem_Calloc(0xE8, 0);
        if (temp_v0_2 != NULL) {
            arg0->work          = (TaskIdMap*)temp_v0_2;
            temp_v0_2->field_E0 = 0;
            sp10                = D_actor_403600_80131E2C;
            Gp_CopyCoordOffset(arg0, &((TmdObject*)temp_s2->parent->extra)->field_8[1], &sp10);
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
    temp_s0 = (Actor403600EffectState*)arg0->work;
    if (D_801153F4 == 0) {
        temp_v1_9 = arg0->spawnArg1;
        switch (temp_v1_9) { /* switch 1; irregular */
            case 1:          /* switch 1 */
                temp_v0_9         = temp_s0->field_E4 - 1;
                temp_s0->field_E4 = temp_v0_9;
                if (temp_v0_9 == 0) {
                    temp_a0_5               = ((Actor403600*)arg0->spawnArg2)->field_2C;
                    D_actor_403600_801606A0 = NULL;
                    temp_a0_5->field_C      = (u16)(temp_a0_5->field_C | 0x80);
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
                temp_a1                 = ((Actor403600*)arg0->spawnArg2)->field_2C;
                temp_a0_6               = (s16*)&temp_s0->field_90;
                D_actor_403600_801606A0 = temp_a0_6;
                temp_a1->field_C        = (u16)(temp_a1->field_C & 0xFF7F);
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

    poly  = (POLY_GT3*)arg0->field_0;
    col   = D_actor_403600_80131E34;
    light = arg0->field_80->field_2C;
    if (arg0->field_1C-- > 0) {
        opz         = &arg0->field_28;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&arg0->field_24);
            if (arg0->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (arg0->field_28 > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3_real();
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
                        norms = (u8*)arg0->field_C;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct_real();
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
                                (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->field_14) &
                                 mask);
                    *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                               (s32)arg0->field_14) =
                        (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                    (s32)arg0->field_14) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
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

    poly  = (POLY_GT3*)arg0->field_4;
    light = arg0->field_80->field_2C;
    if (arg0->field_1C-- > 0) {
        opz         = &arg0->field_28;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        mask        = 0xFFFFFF;
        mask_hi     = 0xFF000000;
        do {
            rec = (u16*)arg2;
            actor_403600_ldsxy3_fifo_gt3((u8*)poly + 7);
            gte_nclip_real();
            gte_stopz(opz);
            if (arg0->field_28 > 0) {
                sz_table = (u8*)arg0->field_10;
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
                            gte_avsz3_real();
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
                                        (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                                    (s32)arg0->field_14) &
                                         mask);
                            *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                       (s32)arg0->field_14) =
                                (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                            (s32)arg0->field_14) &
                                 mask_hi) |
                                ((u32)poly & mask);
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
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

    poly  = (POLY_GT4*)arg0->field_0;
    col   = D_actor_403600_80131E34;
    light = arg0->field_80->field_2C;
    gte_ldrgb(&col);
    if (arg0->field_1C-- > 0) {
        flg         = &arg0->field_24;
        clip_mask   = 0x80000000;
        opz         = &arg0->field_28;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if (!(arg0->field_24 & clip_mask)) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if (!(arg0->field_24 & clip_mask)) {
                    if (arg0->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (arg0->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
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
                            norms = (u8*)arg0->field_C;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct_real();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->field_C + (rec[7] & 0xFFF8));
                            gte_nccs_real();
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
                                    (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->field_14) &
                                     mask);
                        *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                   (s32)arg0->field_14) =
                            (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                        (s32)arg0->field_14) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
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

    poly  = (POLY_GT4*)arg0->field_4;
    light = arg0->field_80->field_2C;
    if (arg0->field_1C-- > 0) {
        opz         = &arg0->field_28;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        mask_hi = 0xFF000000;
        do {
            rec = (u16*)arg2;
            actor_403600_ldsxy3_fifo_gt4((u8*)poly + 7);
            gte_nclip_real();
            gte_stopz(opz);
            if (arg0->field_28 > 0) {
                goto draw;
            }
            actor_403600_ldsxy1_fifo_gt4((u8*)poly + 7);
            gte_nclip_real();
            gte_stopz(opz);
            if (arg0->field_28 < 0) {
            draw:
                sz_table = (u8*)arg0->field_10;
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
                                gte_avsz4_real();
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
                                            (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                                        (s32)arg0->field_14) &
                                             mask);
                                *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                           (s32)arg0->field_14) =
                                    (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                                (s32)arg0->field_14) &
                                     mask_hi) |
                                    ((u32)poly & mask);
                            }
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
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

    poly  = (POLY_GT3*)arg0->field_0;
    col   = D_actor_403600_80131E34;
    light = arg0->field_80->field_2C;
    if (arg0->field_1C-- > 0) {
        opz         = &arg0->field_28;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&arg0->field_24);
            if (arg0->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (arg0->field_28 > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3_real();
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
                        norms = (u8*)arg0->field_C;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct_real();
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
                                (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->field_14) &
                                 mask);
                    *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                               (s32)arg0->field_14) =
                        (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                    (s32)arg0->field_14) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
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

    poly  = (POLY_GT3*)arg0->field_0;
    col   = D_actor_403600_80131E34;
    light = arg0->field_80->field_2C;
    if (arg0->field_1C-- > 0) {
        opz         = &arg0->field_28;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&arg0->field_24);
            if (arg0->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (arg0->field_28 > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3_real();
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
                        norms = (u8*)arg0->field_C;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct_real();
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
                                (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->field_14) &
                                 mask);
                    *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                               (s32)arg0->field_14) =
                        (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                    (s32)arg0->field_14) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
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

    poly  = (POLY_GT4*)arg0->field_0;
    col   = D_actor_403600_80131E34;
    light = arg0->field_80->field_2C;
    gte_ldrgb(&col);
    if (arg0->field_1C-- > 0) {
        flg         = &arg0->field_24;
        clip_mask   = 0x80000000;
        opz         = &arg0->field_28;
        upper_limit = 0x168 - light;
        ds          = &Display_State;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if (!(arg0->field_24 & clip_mask)) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if (!(arg0->field_24 & clip_mask)) {
                    if (arg0->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (arg0->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
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
                            norms = (u8*)arg0->field_C;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct_real();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->field_C + (rec[7] & 0xFFF8));
                            gte_nccs_real();
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
                                    (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->field_14) &
                                     mask);
                        *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                   (s32)arg0->field_14) =
                            (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                        (s32)arg0->field_14) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
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
        poly    = (POLY_GT3*)arg0->field_0;
        color   = D_actor_403600_80131E34;
        head    = *(u8**)0x1F8003FC;
        scratch = (*(u8**)0x1F8003FC = head - 0x7C);
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
        gte_mvmva_10030();
        gte_stsv(local_addr);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir_real();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x70) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x74) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x78) = *(s16*)(scratch + 0x14);
        gte_ldrgb(&color);
        if (arg0->field_1C-- > 0) {
            __asm__("move %0,%1" : "=r"(active) : "r"(transposed));
            projected = (MATRIX*)(head - 0x64);
            SOFT_TOUCH_REG(projected);
            opz = &arg0->field_28;
            __asm__("lui %0,%%hi(Display_State)" : "=r"(ds_high));
            __asm__("addiu %0,%1,%%lo(Display_State)" : "=&r"(ds) : "r"(ds_high));
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
                    verts = (u8*)arg0->field_8;
                    gte_ldv0(verts + (*(s32*)(index + 0x30) << 3));
                    gte_mvmva_10000();
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
                gte_rtpt_real();
                gte_stflg(&arg0->field_24);
                if (arg0->field_24 >= 0) {
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (arg0->field_28 > 0) {
                        gte_stsxy3_gt3(poly);
                        gte_avsz3_real();
                        norms = (u8*)arg0->field_C;
                        gte_ldv3(norms + (record[3] & 0xFFF8),
                                 norms + (record[4] & 0xFFF8),
                                 norms + (record[5] & 0xFFF8));
                        gte_ncct_real();
                        gte_strgb3_gt3(poly);
                        setlen(poly, 9);
                        setcode(poly, 0x34);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & mask_hi) |
                                    (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->field_14) &
                                     mask);
                        *(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                   (s32)arg0->field_14) =
                            (*(u_long*)(((((u32)arg0->field_28 << ds->field_128) >> 2) & 0xFFC) +
                                        (s32)arg0->field_14) &
                             mask_hi) |
                            ((u32)poly & mask);
                    }
                }
                poly++;
                stream += arg0->field_18;
            } while (arg0->field_1C-- > 0);
        }
        arg0->field_0 = (u8*)poly;
        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x3C);
        *(u8**)0x1F8003FC = *(u8**)0x1F8003FC + 0x7C;
        return stream;
    }
    return Tmd_StreamHandler_Op38(arg0, arg1, stream);
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
        poly    = (POLY_GT4*)arg0->field_0;
        color   = D_actor_403600_80131E34;
        head    = *(u8**)0x1F8003FC;
        scratch = (*(u8**)0x1F8003FC = head - 0x88);
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
        gte_mvmva_10030();
        gte_stsv(local_addr);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir_real();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x7C) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x80) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x84) = *(s16*)(scratch + 0x14);
        color_ds                = (u8*)&color;
        gte_ldrgb(color_ds);
        if (arg0->field_1C-- > 0) {
            active = transposed;
            SOFT_USE_REG(head);
            flg       = &arg0->field_24;
            clip_mask = 0x80000000;
            opz       = &arg0->field_28;
            __asm__("lui %0,%%hi(Display_State)" : "=r"(ds_high));
            __asm__("addiu %0,%1,%%lo(Display_State)" : "=&r"(color_ds) : "r"(ds_high));
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
                    verts = (u8*)arg0->field_8;
                    gte_ldv0(verts + (*(s32*)(index + 0x38) << 3));
                    gte_mvmva_10000();
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
                gte_rtpt_real();
                gte_stflg(flg);
                if (!(arg0->field_24 & clip_mask)) {
                    gte_nclip_real();
                    gte_stopz(opz);
                    gte_stsxy3_gt4(poly);
                    gte_ldv0(scratch + 0x30);
                    gte_rtps_real();
                    gte_stflg(flg);
                    if (!(arg0->field_24 & clip_mask)) {
                        if (arg0->field_28 > 0) {
                            goto draw;
                        }
                        gte_nclip_real();
                        gte_stopz(opz);
                        if (arg0->field_28 < 0) {
                        draw:
                            gte_stsxy2(&poly->x3);
                            gte_avsz4_real();
                            norms = (u8*)arg0->field_C;
                            gte_ldv3(norms + (record[4] & 0xFFF8),
                                     norms + (record[5] & 0xFFF8),
                                     norms + (record[6] & 0xFFF8));
                            gte_ncct_real();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->field_C + (record[7] & 0xFFF8));
                            gte_nccs_real();
                            gte_strgb(&poly->r3);
                            setlen(poly, 12);
                            setcode(poly, 0x3C);
                            gte_stotz(opz);
                            poly->tag = (poly->tag & (u32)saved) |
                                        (*(u_long*)(((((u32)arg0->field_28 << ((DisplayState*)color_ds)->field_128) >> 2) &
                                                     0xFFC) +
                                                    (s32)arg0->field_14) &
                                         mask);
                            *(u_long*)(((((u32)arg0->field_28 << ((DisplayState*)color_ds)->field_128) >> 2) & 0xFFC) +
                                       (s32)arg0->field_14) =
                                (*(u_long*)(((((u32)arg0->field_28 << ((DisplayState*)color_ds)->field_128) >> 2) & 0xFFC) +
                                            (s32)arg0->field_14) &
                                 (u32)saved) |
                                ((u32)poly & mask);
                        }
                    }
                }
                poly++;
                stream += arg0->field_18;
            } while (arg0->field_1C-- > 0);
        }
        arg0->field_0 = (u8*)poly;
        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x48);
        *(u8**)0x1F8003FC = *(u8**)0x1F8003FC + 0x88;
        return stream;
    }
    return Tmd_StreamHandler_Op78(arg0, arg1, stream);
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
        if (arg0->field_1C == 0) {
            return (u32*)stream;
        }

        head    = *(u8**)0x1F8003FC;
        scratch = (*(u8**)0x1F8003FC = head - 0x7C);
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
        gte_mvmva_10030();
        gte_stsv(scratch + 0x10);

        gte_SetRotMatrix(transposed);
        gte_ldclmv(saved);
        gte_rtir_real();
        gte_stclmv(transposed);
        gte_ldclmv(head - 0x3E);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 2);
        gte_ldclmv(head - 0x3C);
        gte_rtir_real();
        gte_stclmv((u8*)transposed + 4);

        *(s32*)(scratch + 0x70) = *(s16*)(scratch + 0x10);
        *(s32*)(scratch + 0x74) = *(s16*)(scratch + 0x12);
        *(s32*)(scratch + 0x78) = *(s16*)(scratch + 0x14);
        gte_ldrgb(&color);

        if (arg0->field_1C-- > 0) {
            active = transposed;
            saved  = (MATRIX*)(head - 0x64);
            do {
                record = stream;
                if (*(u16*)(stream + 0) != previous) {
                    gte_SetTransMatrix(active);
                    gte_SetRotMatrix(active);
                    gte_ldv0((u8*)arg0->field_8 + (*(u16*)(stream + 0) & 0xFFF8));
                    gte_mvmva_10000();
                    gte_stsv(saved);
                    if (*(s16*)(scratch + 0x1A) > 0) {
                        *(s16*)(scratch + 0x1A) = 0;
                    }
                    gte_SetRotMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                    gte_SetTransMatrix((u8*)D_actor_403600_801606A0 + 0x24);
                    gte_ldv0(saved);
                    gte_rtps_real();
                    gte_stsz(&arg0->field_28);
                    gte_stflg(&arg0->field_24);
                    if (arg0->field_24 & 0x80000000) {
                        arg0->field_28 |= 0x80000000;
                    }
                    arg0->field_10[*(u16*)(stream + 0) >> 3] = arg0->field_28;
                }
                gte_stsxy(arg0->field_4 + *(u16*)(record + 4));
                gte_ldv0((u8*)arg0->field_C + (*(u16*)(record + 2) & 0xFFF8));
                gte_nccs_real();
                colorOffset = *(u16*)(record + 6);
                stream     += arg0->field_18 * 4;
                gte_strgb(arg0->field_4 + colorOffset);
                loadedCount = arg0->field_1C;
                SOFT_USE_REG(loadedCount);
                previous = *(u16*)(record + 0);
                __asm__ volatile("move %0,%1" : "=r"(count) : "r"(loadedCount));
                arg0->field_1C = loadedCount - 1;
            } while (count > 0);
        }

        gte_SetTransVector(scratch);
        gte_SetRotMatrix(scratch + 0x3C);
        *(u8**)0x1F8003FC = *(u8**)0x1F8003FC + 0x7C;
        return (u32*)stream;
    }
    return Tmd_StreamHandler_OpC8(arg0, arg1, (u32*)stream);
}

void func_actor_403600_80138C34(Task* arg0)
{
    Task* parent;

    parent = arg0->parent;
    func_actor_403600_80132E40(parent, (Actor403600Work*)parent->parent->work, (Actor403600Work*)parent->work);
}

void func_actor_403600_80138C68(Task* arg0)
{
    Gp_UnlinkObj(&((Actor403600Work*)arg0->work)->obj);
    Task_Kill(arg0);
}

void func_actor_403600_80138C9C(Actor403600MotionState* arg0)
{
    s32  temp_a1;
    s32  temp_a1_2;
    s32  temp_v0;
    s32  temp_v0_3;
    s32  temp_v1;
    s32  temp_v1_2;
    s32  var_v0;
    s16* temp_v0_2;
    s16* temp_v0_4;

    temp_a1        = arg0->field_80;
    temp_v1        = temp_a1 + 0x1F;
    var_v0         = temp_v1;
    arg0->field_80 = temp_v1;
    if (temp_v1 < 0) {
        var_v0 = temp_a1 + 0x3E;
    }
    temp_v0 = temp_v1 - ((var_v0 >> 5) << 5);
    __asm__("move %0,%1" : "=r"(temp_a1_2) : "r"(temp_v0));
    arg0->field_80  = temp_v0;
    temp_v0_2       = &arg0->field_0[temp_a1_2];
    temp_v0_2[0]    = 0;
    temp_v0_2[0x20] = 0;
    if (arg0->field_8E != 0) {
        if (arg0->field_8C == 0) {
            arg0->field_84 = 0;
        }
        temp_v1_2 = arg0->field_88;
        if (temp_v1_2 < 0x1000) {
            arg0->field_88 = temp_v1_2 + 0x200;
        }
    } else {
        temp_v0_3 = arg0->field_88;
        if (temp_v0_3 > 0) {
            arg0->field_88 = temp_v0_3 - 0x80;
        }
    }
    arg0->field_8C = (u16)arg0->field_8E;
    if (arg0->field_88 != 0) {
        temp_v0_4       = &arg0->field_0[temp_a1_2];
        temp_v0_4[0]    = (u16)arg0->field_84;
        temp_v0_4[0x20] = (u16)arg0->field_88;
        if (arg0->field_E0 == 0) {
            arg0->field_84 += 0x180;
            return;
        }
        arg0->field_84 += 0x100;
    }
}

s32 func_actor_403600_80138D9C(s16* arg0)
{
    s32 i;

    i = 0;
loop:
    i++;
    if (*arg0 == 0) {
        arg0++;
        if (i < 0x20) {
            goto loop;
        }
        return 1;
    }
    return 0;
}

u8* func_actor_403600_80138DCC(Actor403600* arg0)
{
    u8*                        head;
    u8*                        restore;
    Actor403600ProjectScratch* block;
    TmdObject*                 object;
    GsCOORDINATE2*             coord;
    SVECTOR*                   vec;

    object = arg0->field_2C;
    actor_403600_load_scratch_head(head);
    coord = object->field_8;
    SOFT_BARRIER();
    block = (Actor403600ProjectScratch*)(head - sizeof(Actor403600ProjectScratch));
    actor_403600_store_scratch_head(block);
    block->vec.vx = 0;
    block->vec.vy = 0;
    block->vec.vz = 0;
    Gp_UpdateCoord(&coord[1]);
    vec = &block->vec;
    SOFT_TOUCH_REG(vec);
    gte_SetRotMatrix(&coord[1].workm);
    gte_SetTransMatrix(&coord[1].workm);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    func_actor_403600_801320F8(block->otz);
    actor_403600_load_scratch_head(restore);
    actor_403600_restore_scratch_head(restore);
    return restore;
}

/// Spawns this actor. `enemy` is the task's spawn argument and the 0x7B8-byte
/// work block is parked in `task->work`; on allocation failure the enemy is
/// destroyed. The coordinate at `((TmdObject*)task->extra)->field_8` takes the
/// work block's `field_4B8` as its `sub`, both are reset to the identity with
/// the coordinate's translation raised to 0x744, and the coordinate's three
/// s32 at 0x18 are copied into the work block's 0x4D0 vector. The enemy is
/// then linked on its 0x10 node and three `GpObj` nodes are linked with
/// `Gp_LinkObj` (kinds 2 and 3) over the 0x508 / 0x588 / 0x5C0 records, each
/// seeded through `Gp_InitRec18Table` from the 0x528 / 0x5A8 / 0x5F8 tables.
/// `enemy->field_40` and `field_78A` take the HP from `D_actor_403600_80150EC8`
/// scaled by the spawn multiplier in `gGameSession`, slots 1..0x13 are reset,
/// the model is faced along the world coordinate through `Gfx_MatrixCol2` /
/// `ratan2` / `RotMatrix`, and the display task is spawned from
/// `D_actor_403600_801421A0` and reparented.
void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    SVECTOR rot;
    s16     temp_a0_2;
    s16     temp_s0_5;
    s32     temp_s5;
    Task*   temp_v0_4;
    s32     var_s0;
    void*   temp_a0;
    void*   temp_s0;
    void*   temp_s0_2;
    void*   temp_s0_3;
    void*   temp_s0_4;
    void*   temp_s2;
    void*   temp_v0;
    void*   temp_v0_2;
    void*   temp_v0_3;
    void*   gpSess;

    temp_s2 = task->extra;
    temp_s0 = ACTOR_FIELD(temp_s2, void**, 8);
    temp_v0 = Mem_Calloc(0x7B8, 0);
    temp_s5 = temp_s0 + 0x50;
    if (temp_v0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                                   = (TaskIdMap*)temp_v0;
    ACTOR_FIELD(temp_v0, GsCOORDINATE2**, 0x504) = &Gfx_ViewCoord;
    temp_v0_2                                    = temp_v0 + 0x4BC;
    ACTOR_FIELD(temp_v0, s32*, 0x4BC)            = 0x1000;
    ACTOR_FIELD(temp_v0_2, s32*, 4)              = 0;
    ACTOR_FIELD(temp_v0_2, s32*, 8)              = 0x1000;
    ACTOR_FIELD(temp_v0_2, s32*, 0xC)            = 0;
    ACTOR_FIELD(temp_v0_2, s16*, 0x10)           = 0x1000;
    ACTOR_FIELD(temp_v0, s32*, 0x4D0)            = (s32)ACTOR_FIELD(temp_s0, s32*, 0x18);
    ACTOR_FIELD(temp_v0, s32*, 0x4D4)            = (s32)ACTOR_FIELD(temp_s0, s32*, 0x1C);
    temp_a0                                      = temp_v0 + 0x4B8;
    ACTOR_FIELD(temp_v0, s32*, 0x4D8)            = (s32)ACTOR_FIELD(temp_s0, s32*, 0x20);
    temp_v0_3                                    = temp_s0 + 4;
    ACTOR_FIELD(temp_s0, void**, 0x4C)           = temp_a0;
    ACTOR_FIELD(temp_s0, s32*, 4)                = 0x1000;
    ACTOR_FIELD(temp_v0_3, s32*, 4)              = 0;
    ACTOR_FIELD(temp_v0_3, s32*, 8)              = 0x1000;
    ACTOR_FIELD(temp_v0_3, s32*, 0xC)            = 0;
    ACTOR_FIELD(temp_v0_3, s16*, 0x10)           = 0x1000;
    ACTOR_FIELD(temp_s0, s32*, 0x18)             = 0;
    ACTOR_FIELD(temp_s0, s32*, 0x1C)             = 0x744;
    ACTOR_FIELD(temp_s0, s32*, 0x20)             = 0;
    ACTOR_FIELD(temp_v0, s32*, 0x4B8)            = 0;
    Gp_UpdateCoord((GsCOORDINATE2*)temp_a0);
    ACTOR_FIELD(temp_s0, s32*, 0) = 0;
    Gp_UpdateCoord((GsCOORDINATE2*)temp_s0);
    ACTOR_FIELD(temp_s2, s16*, 0xC)    = 0x80;
    ACTOR_FIELD(temp_s0, s32*, 0)      = 0;
    ACTOR_FIELD(temp_s2, void**, 0x1C) = (void*)(temp_v0 + 0x494);
    ACTOR_FIELD(temp_s2, void**, 0x20) = (void*)(temp_v0 + 0x474);
    enemy->field_4                     = (MATRIX*)(temp_s0 + 0x54);
    enemy->field_48                    = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4               = 1;
    enemy->field_1C.vy                = -0x1F4;
    gpSess                            = gGameSession;
    enemy->field_18                   = (GsCOORDINATE2*)temp_s5;
    enemy->field_1C.vx                = 0;
    enemy->field_1C.vz                = 0;
    enemy->field_50                   = &D_actor_403600_80150EC8;
    enemy->field_54                   = (s32)(temp_v0 + 0x528);
    temp_a0_2                         = ACTOR_FIELD(&D_actor_403600_80150EC8, u16*, 4) + ((ACTOR_FIELD(gpSess, u16*, 0x12A) * 0x4B) / 100);
    enemy->field_40                   = temp_a0_2;
    ACTOR_FIELD(temp_v0, s16*, 0x78A) = temp_a0_2;
    var_s0                            = 1;
    ACTOR_FIELD(temp_v0, s16*, 0x798) = (s16)((temp_a0_2 * 0x3C) / 100);
    ACTOR_FIELD(temp_v0, s16*, 0x79A) = (s16)((ACTOR_FIELD(temp_v0, s16*, 0x78A) * 0x23) / 100);
    func_800B3F84((GpAnimCtx*)temp_v0, D_actor_403600_8016057C, (GpAnimObj*)temp_s2,
                  temp_v0 + 0x334, (GpAnimSlot*)(temp_v0 + 0x14));
    do {
        Gp_AnimResetSlot(temp_v0, var_s0, 1);
        var_s0 += 1;
    } while (var_s0 < 0x14);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    ACTOR_FIELD(temp_v0, s16*, 0x736)   = 1;
    ACTOR_FIELD(temp_v0, void**, 0x658) = (void*)(temp_v0 + 0x4B8);
    ACTOR_FIELD(temp_v0, s16*, 0x65C)   = 0x600;
    ACTOR_FIELD(temp_v0, s16*, 0x65E)   = 2;
    ACTOR_FIELD(temp_v0, s16*, 0x6EA)   = -0x1F4;
    temp_s0_2                           = temp_v0 + 0x528;
    ACTOR_FIELD(temp_v0, s16*, 0x738)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x744)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x6E8)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x6EC)   = 0xC8;
    ACTOR_FIELD(temp_v0, s32*, 0x510)   = temp_s5;
    ACTOR_FIELD(temp_v0, void**, 0x514) = temp_s0_2;
    ACTOR_FIELD(temp_v0, s16*, 0x518)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x51A)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x51C)   = 0;
    ACTOR_FIELD(temp_v0, s32*, 0x520)   = 0x30024;
    ACTOR_FIELD(temp_v0, s16*, 0x524)   = 0x3E8;
    ACTOR_FIELD(temp_v0, u16*, 0x526)   = 1U;
    Gp_LinkObj(2, (GpObj*)((u8*)temp_v0 + 0x508));
    Gp_InitRec18Table((GpRec18*)temp_s0_2, 4, 0);
    temp_s0_3                           = temp_v0 + 0x5A8;
    ACTOR_FIELD(temp_v0, s32*, 0x590)   = temp_s5;
    ACTOR_FIELD(temp_v0, void**, 0x594) = temp_s0_3;
    ACTOR_FIELD(temp_v0, s16*, 0x598)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x59A)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x59C)   = 0x3E8;
    ACTOR_FIELD(temp_v0, u16*, 0x526)   = (u16)(ACTOR_FIELD(temp_v0, u16*, 0x526) | 0xC200);
    ACTOR_FIELD(temp_v0, s32*, 0x5A0)   = Gp_PackPair(&D_actor_403600_80150E9C, 1);
    ACTOR_FIELD(temp_v0, s16*, 0x5A4)   = 0x5DC;
    ACTOR_FIELD(temp_v0, u16*, 0x5A6)   = 1U;
    Gp_LinkObj(3, (GpObj*)((u8*)temp_v0 + 0x588));
    Gp_InitRec18Table((GpRec18*)temp_s0_3, 1, 0);
    temp_s0_4                           = temp_v0 + 0x5F8;
    ACTOR_FIELD(temp_v0, s32*, 0x5C8)   = temp_s5;
    ACTOR_FIELD(temp_v0, void**, 0x5CC) = temp_s0_4;
    ACTOR_FIELD(temp_v0, s16*, 0x5D0)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x5D4)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x5D2)   = 0x7D0;
    ACTOR_FIELD(temp_v0, s32*, 0x5D8)   = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x5DC)   = 0x64;
    ACTOR_FIELD(temp_v0, u16*, 0x5DE)   = 1U;
    ACTOR_FIELD(temp_v0, u16*, 0x5A6)   = (u16)(ACTOR_FIELD(temp_v0, u16*, 0x5A6) & 0x7FFF);
    Gp_LinkObj(3, (GpObj*)((u8*)temp_v0 + 0x5C0));
    Gp_InitRec18Table((GpRec18*)temp_s0_4, 4, 0);
    ACTOR_FIELD(temp_v0, u16*, 0x5DE) = (u16)(ACTOR_FIELD(temp_v0, u16*, 0x5DE) & 0x3FFF);
    Gfx_MatrixCol2((SVECTOR*)(ACTOR_FIELD(ACTOR_FIELD(task, void**, 0x2C), void**, 8) + 4), &rot);
    temp_s0_5 = ratan2(rot.vx, rot.vz);
    rot.vx    = 0;
    rot.vy    = temp_s0_5;
    rot.vz    = 0;
    RotMatrix(&rot, (MATRIX*)(temp_v0 + 0x4BC));
    ACTOR_FIELD(temp_v0, s16*, 0x748) = temp_s0_5;
    temp_v0_4                         = Task_SpawnFromTable(&D_actor_403600_801421A0, 0, 0, 0);
    D_actor_403600_801606AC           = temp_v0_4;
    if (temp_v0_4 != 0) {
        Task_Reparent(task, temp_v0_4);
    }
    ACTOR_FIELD(temp_v0, s32*, 0x4B4) = 0;
    D_actor_403600_801606A8           = task;
    ACTOR_FIELD(temp_v0, s16*, 0x78C) = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x796) = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x7A0) = 0;
    ACTOR_FIELD(temp_v0, s16*, 0x7AE) = 0;
    func_actor_403600_8014174C(task);
    D_actor_403600_8016056C           = 0;
    D_actor_403600_801606B8.field_2   = 0;
    D_actor_403600_801606B8.field_0   = 0;
    task->field_24                    = D_actor_403600_80160504;
    task->exitCallback                = func_actor_403600_80141598;
    ACTOR_FIELD(temp_v0, s16*, 0x730) = 0;
    D_actor_403600_801606BC           = 0;
    task->state                       = (s32)(task->state + 1);
}

void ActorsShared80131e24Sub1(Actor403600Ctx* arg0, Actor403600* arg1)
{
    s16              temp_a1;
    s16              temp_v0;
    s16              temp_v0_2;
    s32              state;
    Actor403600Work* work;
    GsCOORDINATE2*   var_a0;

    state = D_801153F4;
    work  = arg1->field_1C;
    if (state == 1) {
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
    if (work->field_7AC != 0) {
        work->field_7AC = 0;
        SndEvt_EnqueueType9(0x50000000);
    }
    goto default_body;
case1:
    func_actor_403600_801412D0(arg0, arg1);
    if (work->field_7AC == 0) {
        work->field_7AC = state;
        SndEvt_EnqueueType8(0x50000000);
    }
    return;
case2:
    arg1->field_2C->field_C = 0x80;
    arg0->field_14          = 9;
    return;
default_body:
    if (((D_80071075 & 0xF0) == 0x40) && (work->field_7AC == 0)) {
        work->field_7AC = 1;
        SndEvt_EnqueueType8(0x50000000);
    }
    func_actor_403600_801396F8(arg1);
    temp_v0 = work->field_730;
    if (temp_v0 != 0) {
        if (temp_v0 < 0xA) {
            func_actor_403600_8013DC7C(arg1);
            func_actor_403600_8013955C(arg1);
            func_actor_403600_8013D15C(arg1);
        }
    }
    func_actor_403600_801411D4(arg1, 0x14);
    temp_v0_2 = work->field_730;
    if (temp_v0_2 != 0) {
        var_a0 = &work->field_4B8;
        if (temp_v0_2 < 0xA) {
            func_actor_403600_80141338(arg1);
            func_actor_403600_8014161C(arg1);
            func_actor_403600_80141A34(arg1);
            goto block_21;
        }
    } else {
    block_21:
        var_a0 = &work->field_4B8;
    }
    work->field_4B8.flg = 0;
    Gp_UpdateCoord(var_a0);
    func_actor_403600_801412D0(arg0, arg1);
    temp_a1 = work->field_77A;
    if (temp_a1 != 0) {
        Gp_SetObjTrans((GpObj20*)arg1->field_2C, temp_a1, temp_a1, temp_a1);
    }
    func_actor_403600_801414FC(arg1);
    func_actor_403600_8013F0C0(arg1);
}

void func_actor_403600_8013955C(Actor403600* arg0)
{
    Actor403600Work* temp_s1;
    GpEnemy*         temp_s0;
    s32              temp_ret;
    u32              temp_v0;
    u32              temp_v0_2;
    u32              temp_v1_2;
    u8               temp_v1;

    temp_s0 = arg0->field_20;
    temp_v1 = temp_s0->field_4C;
    temp_s1 = arg0->field_1C;
    if (temp_v1 != 0) {
        if (temp_v1 & 1) {
            temp_s0->field_4C  = temp_v1 & 0xFE;
            temp_s1->field_730 = 2;
        }
        if (temp_s0->field_4C & 2) {
            temp_s0->field_4C &= 0xFD;
            temp_s1->field_730 = 3;
            temp_s1->field_736 = 0xE;
            SOFT_BARRIER();
            temp_s1->field_790 = D_actor_403600_80150ED4 * 0x1E;
        }
        if (temp_s0->field_4C & 0xC) {
            if (temp_s1->field_73E != 0x28) {
                temp_ret = Gp_TickObjFlag4((GpObj5C*)temp_s0);
                if (temp_ret != 0) {
                    temp_v1_2   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = temp_v1_2;
                    if ((temp_v1_2 >> 0x10) & 1) {
                        temp_v0            = (temp_v1_2 * 5) + 0x71357911;
                        Gp_LcgState        = temp_v0;
                        temp_s1->field_700 = ((temp_v0 >> 0xB) & 0x60) + 0x80;
                    } else {
                        temp_v0_2          = (temp_v1_2 * 5) + 0x71357911;
                        Gp_LcgState        = temp_v0_2;
                        temp_s1->field_700 = -(((temp_v0_2 >> 0xB) & 0x60) + 0x80);
                    }
                    func_actor_403600_8013DAF4(arg0, temp_ret / 5);
                }
            }
            if ((Gp_ObjFlag4Expired((GpObj5C*)temp_s0) != 0) || (temp_s0->field_40 < 0x1F4)) {
                temp_s0->field_4C &= 0xF3;
            }
        }
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_80131E34);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801396F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013A444);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013C864);

void func_actor_403600_8013CCEC(Actor403600* arg0, s32 arg1)
{
    s32                         temp_a0;
    s32                         temp_v0;
    s32                         temp_v0_10;
    s32                         temp_v0_2;
    s32                         temp_v0_3;
    s32                         temp_v0_4;
    s32                         temp_v0_5;
    s32                         temp_v0_6;
    s32                         temp_v0_7;
    s32                         temp_v0_8;
    s32                         temp_v0_9;
    s32                         temp_v1;
    s32                         temp_v1_2;
    s32                         temp_v1_3;
    s32                         temp_v1_4;
    s32                         temp_v1_5;
    s32                         temp_v1_6;
    s32                         temp_v1_7;
    s32                         temp_v1_8;
    s32                         temp_v1_9;
    s32                         var_a1;
    s32                         var_a2;
    s32                         var_v1;
    Actor403600DistanceScratch* temp_s2;
    Actor403600DistanceScratch* temp_s3;
    Actor403600DistanceScratch* temp_s5;
    Actor403600Work*            temp_s4;
    Actor403600Point*           temp_v0_11;
    Actor403600Point*           temp_v1_10;

    temp_s2             = *(void**)0x1F8003FC;
    temp_s3             = (Actor403600DistanceScratch*)((u8*)temp_s2 - 0x24);
    *(void**)0x1F8003FC = temp_s3;
    temp_s4             = arg0->field_1C;
    temp_s5             = temp_s3;
    if (arg1 == 0) {
        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[0].x;
        temp_v1               = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[0].z;
        temp_s3->z            = temp_v1;
        temp_v0               = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0 * temp_v0) + (temp_v1 * temp_v1));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[1].x;
        temp_v1_2             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[1].z;
        temp_s3->z            = temp_v1_2;
        temp_v0_2             = temp_s3->x;
        temp_s3->distances[1] = SquareRoot0((temp_v0_2 * temp_v0_2) + (temp_v1_2 * temp_v1_2));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[2].x;
        temp_v1_3             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[2].z;
        temp_s3->z            = temp_v1_3;
        temp_v0_3             = temp_s3->x;
        temp_s3->distances[2] = SquareRoot0((temp_v0_3 * temp_v0_3) + (temp_v1_3 * temp_v1_3));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[3].x;
        temp_v1_4             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[3].z;
        temp_s3->z            = temp_v1_4;
        temp_v0_4             = temp_s3->x;
        temp_s3->distances[3] = SquareRoot0((temp_v0_4 * temp_v0_4) + (temp_v1_4 * temp_v1_4));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[4].x;
        temp_v1_5             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[4].z;
        temp_s3->z            = temp_v1_5;
        temp_v0_5             = temp_s3->x;
        temp_s3->distances[4] = SquareRoot0((temp_v0_5 * temp_v0_5) + (temp_v1_5 * temp_v1_5));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[5].x;
        temp_v1_6             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[5].z;
        temp_s3->z            = temp_v1_6;
        temp_v0_6             = temp_s3->x;
        temp_s3->distances[5] = SquareRoot0((temp_v0_6 * temp_v0_6) + (temp_v1_6 * temp_v1_6));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[6].x;
        temp_v1_7             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[6].z;
        temp_s3->z            = temp_v1_7;
        temp_v0_7             = temp_s3->x;
        temp_s3->distances[6] = SquareRoot0((temp_v0_7 * temp_v0_7) + (temp_v1_7 * temp_v1_7));
        var_a2                = 0xFFFFFF;
        var_v1                = 0;
        var_a1                = 0;
        do {
            temp_a0 = temp_s5->distances[var_a1 & 0xFF];
            if (temp_a0 < var_a2) {
                var_v1 = var_a1;
                var_a2 = temp_a0;
            }
            var_a1 += 1;
        } while ((u32)(var_a1 & 0xFF) < 7U);
    } else {
        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[7].x;
        temp_v1_8             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[7].z;
        temp_s3->z            = temp_v1_8;
        temp_v0_8             = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0_8 * temp_v0_8) + (temp_v1_8 * temp_v1_8));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[8].x;
        temp_v1_9             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[8].z;
        temp_s3->z            = temp_v1_9;
        temp_v0_9             = temp_s3->x;
        temp_v0_10            = SquareRoot0((temp_v0_9 * temp_v0_9) + (temp_v1_9 * temp_v1_9));
        temp_s3->distances[1] = temp_v0_10;
        var_v1                = temp_v0_10 >= temp_s3->distances[0];
    }
    temp_v0_11 = D_actor_403600_801605F4;
    temp_v1_10 = (var_v1 & 0xFF) + temp_v0_11;
    do {
        temp_s4->field_6F0.vx = temp_v1_10->x;
    } while (0);
    temp_s4->field_6F0.vy = D_80073B8C->t[1] - 0x258;
    *(void**)0x1F8003FC   = (u8*)*(void**)0x1F8003FC + 0x24;
    temp_s4->field_6F0.vz = temp_v1_10->z;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013D15C);

s32 func_actor_403600_8013D9A8(Actor403600* arg0)
{
    s32              i;
    s32              mask;
    s32              kind;
    s32              callResult;
    s32              three;
    s32              x;
    s32              y;
    s32              z;
    Actor403600Work* work;
    u8*              entry;

    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x10;
    work  = arg0->field_1C;
    entry = (u8*)work;
    do {
        if ((((u32)(((volatile GpRec18*)(entry + 0x5F8))->field_4 & mask)) >> 16) == kind) {
            if (work->field_786 == 0) {
                work->field_786++;
                callResult = func_800E1B24(((volatile GpRec18*)(entry + 0x5F8))->field_4);
                three      = 3;
                if (callResult == three) {
                    return 2;
                }
                SOFT_USE_REG(three);
            }
        }
        i++;
        entry += sizeof(GpRec18);
    } while (i < 4);

    Gp_ClearRec18Occupied(work->field_5F8);
    x = work->field_4B8.coord.t[0] + ((work->field_4B8.coord.m[0][2] * 0x177) >> 9);
    y = work->field_4B8.coord.t[2] + ((work->field_4B8.coord.m[2][2] * 0x177) >> 9);
    if (x < 0x101) {
        return 1;
    }
    if ((x >= 0x3D00) || (y >= 0x3800)) {
        return 1;
    }
    if (y < -0x7F) {
        return 1;
    }
    z = work->field_4B8.coord.t[1];
    if (z >= 0) {
        return 1;
    }
    if (z < -0x176F) {
        return 3;
    }
    return 0;
}

void func_actor_403600_8013DAF4(Actor403600* arg0, s32 arg1)
{
    Actor403600**    temp_v0;
    GpEnemy*         temp_s0;
    Actor403600Work* temp_s1;
    Actor403600*     temp_v0_2;
    Actor403600Work* temp_v0_3;

    temp_s0           = arg0->field_20;
    temp_s1           = arg0->field_1C;
    temp_s0->field_40 = (u16)temp_s0->field_40 - arg1;
    func_800DA6E8(&temp_s0->node, arg1, 0);
    if (temp_s0->field_40 <= 0) {
        if (D_80073BA0 <= 0) {
            temp_s0->field_40 = 0xA;
            return;
        }
        temp_v0 = temp_s1->field_4B4;
        if (temp_v0 != NULL) {
            temp_v0_2                      = *temp_v0;
            temp_v0_2->field_30            = 2;
            temp_v0_2->field_2A            = 0;
            temp_v0_2->field_1C->field_732 = 1;
        }
        temp_s1->field_588.flags &= 0x7FFF;
        Gp_PulseState1C80();
        gGameSession->eventState = 1;
        D_actor_403600_8016056C  = 0;
        Gp_DispatchMsg(*Gp_ActorSlots, 0x3F1, 0, 0);
        temp_v0_3            = arg0->field_1C;
        temp_v0_3->field_756 = 8;
        temp_v0_3->field_776 = 0xA;
        temp_v0_3->field_742 = 0;
        temp_v0_3->field_746 = 0;
        temp_v0_3->field_774 = 0;
        temp_v0_3->field_778 = 0x10;
        temp_v0_3->field_77A = 0;
        temp_v0_3->field_784 = 0;
        temp_v0_3->field_73C = 0;
        temp_v0_3->field_73E = 0;
        temp_v0_3->field_74A = 0;
        temp_v0_3->field_73A = 0;
        temp_v0_3->field_76E = 0x40;
        temp_v0_3->field_75E = 0;
        temp_v0_3->field_7A4 = 0;
        temp_v0_3->field_7A6 = 0;
        temp_v0_3->field_7AC = 0;
        func_actor_403600_80141954(0);
        func_actor_403600_80141B24(arg0);
        temp_s1->field_742 = 1;
        temp_s1->field_736 = 1;
        temp_s1->field_756 = 0;
        temp_s1->field_778 = 0x10;
        temp_s1->field_730 = 0x14;
        temp_s1->field_732 = 0;
        temp_s1->field_7A2 = 0;
        Gp_HaltPadScripts();
        SndEvt_EnqueueType7(0x54160001, 1);
    }
}

void func_actor_403600_8013DC7C(Actor403600* arg0)
{
    s16              temp_v0_2;
    s16              temp_v0;
    u16              var_a3;
    s16              var_a3_signed;
    Actor403600Work* temp_a2;

    temp_a2               = arg0->field_1C;
    var_a3                = temp_a2->field_73C;
    temp_a2->field_6B0.vx = temp_a2->field_4B8.coord.t[0];
    temp_a2->field_6B0.vy = temp_a2->field_4B8.coord.t[1];
    temp_a2->field_6B0.vz = temp_a2->field_4B8.coord.t[2];
    var_a3_signed         = var_a3;
    if ((temp_a2->field_78C != 0) && (var_a3_signed != 0)) {
        var_a3 = (var_a3_signed * 0x3C) / 100;
    }
    if (temp_a2->field_784 != 0) {
        temp_a2->field_4B8.coord.t[0] +=
            (temp_a2->field_4B8.coord.m[0][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[1] +=
            (temp_a2->field_4B8.coord.m[1][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[2] +=
            (temp_a2->field_4B8.coord.m[2][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
    } else {
        temp_a2->field_4B8.coord.t[1] += temp_a2->field_74A;
        temp_a2->field_4B8.coord.t[0] +=
            (temp_a2->field_4B8.coord.m[0][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[2] +=
            (temp_a2->field_4B8.coord.m[2][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
    }
    temp_v0            = temp_a2->field_7A0 - 1;
    temp_a2->field_7A0 = temp_v0;
    if (temp_v0 < 0) {
        temp_v0_2          = (u16)temp_a2->field_796 - 1;
        temp_a2->field_796 = temp_v0_2;
        if (temp_v0_2 < 0) {
            temp_a2->field_796 = 0;
        }
        temp_a2->field_7A0 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DDF4);

s32 func_actor_403600_8013DFE0(Actor403600* arg0)
{
    s16                     temp_v0;
    s16                     temp_v0_2;
    s16                     temp_v0_3;
    s16                     temp_v1_3;
    s16                     temp_v1_4;
    s32                     temp_v1_5;
    s16                     temp_v1_6;
    s32                     temp_v1_7;
    s16                     temp_v1_8;
    s32                     temp_v1_9;
    s16                     var_a0_2;
    s16                     var_a0_3;
    s16                     var_a0_4;
    s32                     var_a1;
    s32                     var_a1_2;
    s32                     var_a1_3;
    s16                     var_v0;
    s16                     var_v0_2;
    s32                     var_v0_3;
    s16                     var_v0_4;
    s32                     var_v0_5;
    s16                     var_v0_6;
    s32                     var_v0_7;
    s32                     temp_lo;
    s32                     temp_s5;
    s32                     temp_step;
    s32                     temp_step_2;
    s32                     temp_step_3;
    u16                     temp_a3;
    u16                     temp_a3_2;
    u16                     temp_a3_3;
    Actor403600TurnMatrix*  temp_s0;
    MATRIX*                 temp_s0_2;
    Actor403600TurnMatrix*  temp_s0_3;
    MATRIX*                 temp_s0_4;
    SVECTOR*                temp_s1;
    SVECTOR*                temp_s2;
    Actor403600Work*        temp_s4;
    Actor403600TurnScratch* temp_v1;
    Actor403600TurnScratch* temp_v1_2;

    temp_v1             = *(Actor403600TurnScratch**)0x1F8003FC;
    temp_v1             = (Actor403600TurnScratch*)((u8*)temp_v1 - 0x30);
    *(void**)0x1F8003FC = temp_v1;
    temp_v1_2           = temp_v1;
    temp_s4             = arg0->field_1C;
    temp_v1_3           = temp_s4->field_746;
    switch (temp_v1_3) {
        case 0:
            temp_v1_2->vector[0] =
                (s16)(Player_Status.coordMtx->t[0] - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] =
                (s16)(Player_Status.coordMtx->t[1] - temp_s4->field_4B8.coord.t[1]);
            temp_v1_2->vector[2] =
                (s16)(Player_Status.coordMtx->t[2] - temp_s4->field_4B8.coord.t[2]);
            break;
        case 1:
        case 3:
            temp_v1_2->vector[0] = (s16)(temp_s4->field_6F0.vx - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] = (s16)(temp_s4->field_6F0.vy - temp_s4->field_4B8.coord.t[1]);
            temp_v1_2->vector[2] = (s16)(temp_s4->field_6F0.vz - temp_s4->field_4B8.coord.t[2]);
            break;
        case 2:
            temp_s5              = (s16)(Player_Status.coordMtx->t[0] - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] = 0;
            temp_v1_2->vector[0] = temp_s5;
            temp_v1_2->vector[2] =
                (s16)(Player_Status.coordMtx->t[2] - temp_s4->field_4B8.coord.t[2]);
            break;
    }
    temp_v0   = temp_v1_2->vector[0];
    temp_v0_2 = temp_v1_2->vector[1];
    temp_v0_3 = temp_v1_2->vector[2];
    temp_lo   = temp_v0_3 * temp_v0_3;
    temp_s5   = SquareRoot0((temp_v0 * temp_v0) + (temp_v0_2 * temp_v0_2) + temp_lo);
    if (temp_s4->field_746 == 3) {
        temp_s2                   = (SVECTOR*)temp_v1_2->vector;
        temp_s0                   = &temp_v1_2->matrix;
        temp_v1_2->matrix.field_0 = 0x1000;
        temp_s0->field_4          = 0;
        temp_s0->field_8          = 0x1000;
        temp_s0->field_C          = 0;
        temp_s0->field_10         = 0x1000;
        VectorNormalSS(temp_s2, temp_s2);
        temp_v1_2->angles[0] = 0;
        temp_v1_2->angles[1] = 0x1000;
        temp_v1_2->angles[2] = 0;
        Gfx_OrthonormalBasis((MATRIX*)temp_s0, temp_s2, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler((MATRIX*)temp_s0, (SVECTOR*)temp_v1_2);
        temp_s0_2            = &temp_s4->field_4B8.coord;
        temp_v1_2->angles[2] = (u16)(temp_v1_2->angles[2] + temp_s4->field_75E);
        Gfx_RotMatrixXYZ(temp_s0_2, (SVECTOR*)temp_v1_2, 1);
        Gfx_MatrixCol2(temp_s0_2, (SVECTOR*)temp_v1_2);
    } else {
        temp_s1                   = (SVECTOR*)temp_v1_2->vector;
        temp_s0_3                 = &temp_v1_2->matrix;
        temp_v1_2->matrix.field_0 = 0x1000;
        temp_s0_3->field_4        = 0;
        temp_s0_3->field_8        = 0x1000;
        temp_s0_3->field_C        = 0;
        temp_s0_3->field_10       = 0x1000;
        VectorNormalSS(temp_s1, temp_s1);
        temp_v1_2->angles[0] = 0;
        temp_v1_2->angles[1] = 0x1000;
        temp_v1_2->angles[2] = 0;
        Gfx_OrthonormalBasis((MATRIX*)temp_s0_3, temp_s1, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler((MATRIX*)temp_s0_3, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler(&temp_s4->field_4B8.coord, temp_s1);
        temp_a3   = temp_v1_2->angles[0];
        temp_v1_4 = (temp_a3 & 0xFFF) - ((u16)temp_v1_2->vector[0] & 0xFFF);
        temp_step = temp_s4->field_76E;
        var_a1    = temp_v1_4;
        if (temp_v1_4 < 0) {
            SOFT_TOUCH_REG(var_a1);
            var_a1 = -var_a1;
        }
        var_a0_2 = temp_v1_4;
        if (temp_step >= var_a1) {
            temp_v1_2->vector[0] = (s16)temp_a3;
        } else {
            SOFT_TOUCH_REG(var_a0_2);
            SOFT_TOUCH_REG(var_a0_2);
            if (var_a1 >= 0x801) {
                var_v0_2 = temp_v1_4 - 0x1000;
                if (temp_v1_4 <= 0) {
                    var_v0_2 = 0x1000 - temp_v1_4;
                }
                var_a0_2 = var_v0_2;
            }
            temp_v1_5 = temp_v1_2->vector[0];
            if ((var_a0_2 << 0x10) > 0) {
                var_v0_3 = temp_v1_5 + temp_s4->field_76E;
            } else {
                var_v0_3 = temp_v1_5 - temp_s4->field_76E;
            }
            temp_v1_2->vector[0] = var_v0_3;
        }
        temp_a3_2   = temp_v1_2->angles[1];
        temp_v1_6   = (temp_a3_2 & 0xFFF) - ((u16)temp_v1_2->vector[1] & 0xFFF);
        temp_step_2 = temp_s4->field_76E;
        var_a1_2    = temp_v1_6;
        if (temp_v1_6 < 0) {
            SOFT_TOUCH_REG(var_a1_2);
            var_a1_2 = -var_a1_2;
        }
        var_a0_3 = temp_v1_6;
        if (temp_step_2 >= var_a1_2) {
            temp_v1_2->vector[1] = (s16)temp_a3_2;
        } else {
            SOFT_TOUCH_REG(var_a0_3);
            SOFT_TOUCH_REG(var_a0_3);
            if (var_a1_2 >= 0x801) {
                var_v0_4 = temp_v1_6 - 0x1000;
                if (temp_v1_6 <= 0) {
                    var_v0_4 = 0x1000 - temp_v1_6;
                }
                var_a0_3 = var_v0_4;
            }
            temp_v1_7 = temp_v1_2->vector[1];
            if ((var_a0_3 << 0x10) > 0) {
                var_v0_5 = temp_v1_7 + temp_s4->field_76E;
            } else {
                var_v0_5 = temp_v1_7 - temp_s4->field_76E;
            }
            temp_v1_2->vector[1] = var_v0_5;
        }
        temp_a3_3   = temp_v1_2->angles[2];
        temp_v1_8   = (temp_a3_3 & 0xFFF) - ((u16)temp_v1_2->vector[2] & 0xFFF);
        temp_step_3 = temp_s4->field_76E;
        var_a1_3    = temp_v1_8;
        if (temp_v1_8 < 0) {
            SOFT_TOUCH_REG(var_a1_3);
            var_a1_3 = -var_a1_3;
        }
        var_a0_4 = temp_v1_8;
        if (temp_step_3 >= var_a1_3) {
            temp_v1_2->vector[2] = (s16)temp_a3_3;
        } else {
            SOFT_TOUCH_REG(var_a0_4);
            SOFT_TOUCH_REG(var_a0_4);
            if (var_a1_3 >= 0x801) {
                var_v0_6 = temp_v1_8 - 0x1000;
                if (temp_v1_8 <= 0) {
                    var_v0_6 = 0x1000 - temp_v1_8;
                }
                var_a0_4 = var_v0_6;
            }
            temp_v1_9 = temp_v1_2->vector[2];
            if ((var_a0_4 << 0x10) > 0) {
                var_v0_7 = temp_v1_9 + temp_s4->field_76E;
            } else {
                var_v0_7 = temp_v1_9 - temp_s4->field_76E;
            }
            temp_v1_2->vector[2] = var_v0_7;
        }
        temp_s0_4            = &temp_s4->field_4B8.coord;
        temp_v1_2->vector[2] = (s16)((u16)temp_v1_2->vector[2] + temp_s4->field_75E);
        Gfx_RotMatrixXYZ(temp_s0_4, (SVECTOR*)temp_v1_2->vector, 1);
        temp_s4->field_748 = (u16)temp_v1_2->vector[1];
        Gfx_MatrixCol2(temp_s0_4, (SVECTOR*)temp_v1_2);
    }
    temp_s4->field_748  = ratan2((s16)temp_v1_2->angles[0], (s16)temp_v1_2->angles[2]);
    *(void**)0x1F8003FC = (void*)((u8*)*(void**)0x1F8003FC + 0x30);
    return temp_s5;
}

void func_actor_403600_8013E470(GsCOORDINATE2* arg0, s32* arg1, s32* arg2)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            x;
    s32            z;
    void*          head;
    void*          vec;
    void*          matrix;
    void*          scratch;

    head                      = *(void**)0x1F8003FC;
    coord                     = (*Gp_ActorSlots)->extra->field_8;
    *(s16*)((s8*)head - 0x40) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    vec                       = head - 0x40;
    *(s16*)((s8*)vec + 2)     = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    scratch                   = (*(void**)0x1F8003FC = head - 0x7C);
    *(s16*)((s8*)vec + 4)     = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                    = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);
    local = *(SVECTOR*)vec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    *arg2 = angle;
    if (angle >= 0x801) {
        *arg2 = angle - 0x1000;
    } else if (angle < -0x800) {
        *arg2 = angle + 0x1000;
    }
    x                            = Player_Status.coordMtx->t[0] - arg0->coord.t[0];
    *(s32*)((s8*)scratch + 0x20) = x;
    *(s32*)((s8*)scratch + 0x24) = Player_Status.coordMtx->t[1] - arg0->coord.t[1];
    z                            = Player_Status.coordMtx->t[2] - arg0->coord.t[2];
    *(s32*)((s8*)scratch + 0x28) = z;
    *arg1                        = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC          = *(void**)0x1F8003FC + 0x7C;
}

s16 func_actor_403600_8013E66C(GsCOORDINATE2* arg0)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s16            angle;
    s16            result;
    void*          vec;
    void*          head;

    head                      = *(void**)0x1F8003FC;
    coord                     = (*Gp_ActorSlots)->extra->field_8;
    *(void**)0x1F8003FC       = head - 0x7C;
    *(s16*)((s8*)head - 0x40) = (s16)(arg0->workm.t[0] - coord->workm.t[0]);
    vec                       = head - 0x40;
    *(s16*)((s8*)vec + 2)     = (s16)(arg0->workm.t[1] - coord->workm.t[1]);
    *(s16*)((s8*)vec + 4)     = (s16)(arg0->workm.t[2] - coord->workm.t[2]);
    TransposeMatrix(&coord->workm, head - 0x20);
    local = *(SVECTOR*)vec;
    gte_SetRotMatrix(head - 0x20);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(vec);
    angle  = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    result = angle;
    if (angle >= 0x801) {
        result = angle - 0x1000;
    } else if (angle < -0x800) {
        result = angle + 0x1000;
    }
    *(void**)0x1F8003FC = *(void**)0x1F8003FC + 0x7C;
    return result;
}

s32 func_actor_403600_8013E7D4(s32 arg0, s32 arg1)
{
    GpActorWork*   temp_s7;
    GsCOORDINATE2* temp_s3;
    s32            temp_s0;
    s32            temp_s0_3;
    s32            temp_s1;
    s32            temp_s5;
    s32            var_s2;
    s32            var_s4;
    s32            var_v1;

    temp_s7 = *Gp_ActorSlots;
    temp_s3 = temp_s7->extra->field_8;
    temp_s1 = D_actor_403600_801605E4.vx - temp_s3->coord.t[0];
    temp_s0 = D_actor_403600_801605E4.vz - temp_s3->coord.t[2];
    var_s4  = 0;
    temp_s5 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_s2  = ratan2(temp_s1, temp_s0);
    if (var_s2 >= 0x801) {
        var_s2 -= 0x1000;
    } else if (var_s2 < -0x800) {
        var_s2 += 0x1000;
    }
    temp_s1   = D_actor_403600_801605EC.vx - temp_s3->coord.t[0];
    temp_s0   = D_actor_403600_801605EC.vz - temp_s3->coord.t[2];
    temp_s0_3 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_v1    = ratan2(temp_s1, temp_s0);
    if (var_v1 >= 0x801) {
        var_v1 -= 0x1000;
    } else if (var_v1 < -0x800) {
        var_v1 += 0x1000;
    }
    if (arg1 & 1) {
        if (temp_s0_3 >= temp_s5) {
            D_actor_403600_801606F2 = (s16)var_v1;
        } else {
            goto block_14;
        }
    } else if (temp_s5 < temp_s0_3) {
    block_14:
        D_actor_403600_801606F2 = (s16)var_s2;
    } else {
        D_actor_403600_801606F2 = (s16)var_v1;
    }
    SOFT_USE_REG(arg1);
    __asm__ volatile(
        ".set\tnoreorder\n\t"
        ".set\tnomacro\n\t"
        "lui $a1, %%hi(D_actor_403600_801606E0)\n\t"
        "addiu $a0, $a1, %%lo(D_actor_403600_801606E0)\n\t"
        "andi $v0, %1, 2\n\t"
        "sw $zero, 0(%2)\n\t"
        "sh $zero, 16($a0)\n\t"
        "beqz $v0, 3f\n\t"
        "sh $zero, 20($a0)\n\t"
        "lw $v0, 28(%2)\n\t"
        "lh $v1, 18($a0)\n\t"
        "nop\n\t"
        "bne $v1, %4, 1f\n\t"
        "sw $v0, 4($a0)\n\t"
        "lui $v0, %%hi(D_actor_403600_801605EC)\n\t"
        "addiu $v1, $v0, %%lo(D_actor_403600_801605EC)\n\t"
        "lh $v0, %%lo(D_actor_403600_801605EC)($v0)\n\t"
        "lh $v1, 4($v1)\n\t"
        "j 2f\n\t"
        "move %0, $zero\n"
        "1:\n\t"
        "lui $v0, %%hi(D_actor_403600_801605E4)\n\t"
        "addiu $v1, $v0, %%lo(D_actor_403600_801605E4)\n\t"
        "lh $v0, %%lo(D_actor_403600_801605E4)($v0)\n\t"
        "lh $v1, 4($v1)\n\t"
        "li %0, 1\n"
        "2:\n\t"
        "sw $v0, %%lo(D_actor_403600_801606E0)($a1)\n\t"
        "j 4f\n\t"
        "sw $v1, 8($a0)\n"
        "3:\n\t"
        "lw $v0, 24(%2)\n\t"
        "nop\n\t"
        "sw $v0, %%lo(D_actor_403600_801606E0)($a1)\n\t"
        "lw $v0, 28(%2)\n\t"
        "nop\n\t"
        "sw $v0, 4($a0)\n\t"
        "lw $v0, 32(%2)\n\t"
        "nop\n\t"
        "sw $v0, 8($a0)\n"
        "4:\n\t"
        "move $a0, %3\n\t"
        "li $a1, 0x3e9\n\t"
        "lui $a2, %%hi(D_actor_403600_801606E0)\n\t"
        "addiu $a2, $a2, %%lo(D_actor_403600_801606E0)\n\t"
        "jal Gp_DispatchMsg\n\t"
        "move $a3, $zero\n\t"
        ".set\tmacro\n\t"
        ".set\treorder"
        : "+r"(var_s4)
        : "r"(arg1), "r"(temp_s3), "r"(temp_s7), "r"(var_s2)
        : "v0", "a0", "a1", "a2", "a3", "memory");
    return var_s4;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013EA04);

void func_actor_403600_8013F0C0(Actor403600* arg0)
{
    s16              temp_v1;
    s16              temp_v1_2;
    s16              temp_v1_3;
    s16              temp_v1_4;
    s32              temp_s2;
    s32              temp_s4;
    s32              var_v0_2;
    s32              temp_s0;
    s32              temp_s0_2;
    s32              temp_s0_3;
    s32              temp_s0_4;
    s32              var_v0;
    u16              temp_v0;
    register u16     temp_v0_2 asm("v0");
    GsCOORDINATE2*   temp_a0;
    GsCOORDINATE2*   temp_a0_2;
    GsCOORDINATE2*   temp_a0_3;
    Actor403600Work* temp_a1;
    Actor403600Work* temp_a1_2;
    Actor403600Work* temp_a1_3;
    Actor403600Work* temp_s3;

    temp_s4 = (s32)Gp_ActorSlots[0]->extra->field_8;
    temp_s3 = arg0->field_1C;
    switch (D_actor_403600_8016056C) {
        case 1:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_a1            = arg0->field_1C;
            temp_a0            = Gp_ActorSlots[0]->extra->field_8;
            temp_a0->coord.t[0] =
                (s32)(temp_a0->coord.t[0] +
                      ((s32)(temp_a0->coord.m[0][2] * temp_a1->field_762) >> 0xC));
            temp_a0->coord.t[2] =
                (s32)(temp_a0->coord.t[2] +
                      ((s32)(temp_a0->coord.m[2][2] * temp_a1->field_762) >> 0xC));
            if (((s16)temp_s3->field_760 >= 0xC) || (gGameSession->viewReady != 0)) {
                temp_s3->field_760 = 0;
                if (func_actor_403600_8013E7D4((s32)arg0, 3) == 0) {
                    D_8007216C = 7;
                } else {
                    D_8007216C = 3;
                }
                D_actor_403600_80160568.field_4 = 2;
                Gp_StateC08.field_6            |= 1;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 2:
            temp_v0            = temp_s3->field_760 + 1;
            temp_s3->field_760 = temp_v0;
            if ((s16)temp_v0 == 0xC) {
                Gp_SpawnPadLerp(0xA, 0xFF, 0xFF);
                D_actor_403600_801606A4.field_0 = 0x14;
                D_actor_403600_801606A4.field_2 = 0;
                temp_s2                         = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 6;
                temp_s0                         = (s8)Gp_GetObjPan((GpObj38*)temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0,
                                    (s8)Gp_GetObjDepth((GpObj38*)temp_s4));
                temp_s2 =
                    (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x54160011;
                temp_s0_2 = (s8)Gp_GetObjPan((GpObj38*)temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_2,
                                    (s8)Gp_GetObjDepth((GpObj38*)temp_s4));
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9,
                               Gp_PackPair(&D_actor_403600_801606A4, 0), 0);
            }
            var_v0_2 = (s16)temp_s3->field_760 < 0x66;
            goto check_timeout;

        case 3:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_v1            = (u16)temp_s3->field_762 + 2;
            temp_s3->field_762 = temp_v1;
            if ((temp_v1 << 0x10) > 0) {
                temp_s3->field_762 = 0;
            }
            temp_a1_2 = arg0->field_1C;
            temp_a0_2 = Gp_ActorSlots[0]->extra->field_8;
            temp_a0_2->coord.t[0] =
                (s32)(temp_a0_2->coord.t[0] +
                      ((s32)(temp_a0_2->coord.m[0][2] * temp_a1_2->field_762) >> 0xC));
            temp_a0_2->coord.t[2] =
                (s32)(temp_a0_2->coord.t[2] +
                      ((s32)(temp_a0_2->coord.m[2][2] * temp_a1_2->field_762) >> 0xC));
            temp_v1_2 = temp_s3->field_73E;
            if ((temp_v1_2 != 0x3C) && (temp_v1_2 != 0x28) &&
                ((s16)temp_s3->field_760 == 0xC)) {
                temp_s2 =
                    (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x54160012;
                temp_s0_3 = (s8)Gp_GetObjPan((GpObj38*)temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_3,
                                    (s8)Gp_GetObjDepth((GpObj38*)temp_s4));
            }
            if ((s16)temp_s3->field_760 >= 0x24) {
                Gp_StateC08.field_6            |= 1;
                temp_s3->field_760              = 0;
                D_actor_403600_80160568.field_4 = 5;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 4:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_v1_3          = (u16)temp_s3->field_762 - 2;
            temp_s3->field_762 = temp_v1_3;
            if (temp_v1_3 < 0) {
                temp_s3->field_762 = 0;
            }
            temp_a1_3 = arg0->field_1C;
            temp_a0_3 = Gp_ActorSlots[0]->extra->field_8;
            temp_a0_3->coord.t[0] =
                (s32)(temp_a0_3->coord.t[0] +
                      ((s32)(temp_a0_3->coord.m[0][2] * temp_a1_3->field_762) >> 0xC));
            temp_a0_3->coord.t[2] =
                (s32)(temp_a0_3->coord.t[2] +
                      ((s32)(temp_a0_3->coord.m[2][2] * temp_a1_3->field_762) >> 0xC));
            temp_v1_4 = temp_s3->field_73E;
            if ((temp_v1_4 != 0x3C) && (temp_v1_4 != 0x28) &&
                ((s16)temp_s3->field_760 == 0xC)) {
                temp_s2 =
                    (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x54160012;
                temp_s0_4 = (s8)Gp_GetObjPan((GpObj38*)temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_4,
                                    (s8)Gp_GetObjDepth((GpObj38*)temp_s4));
            }
            if ((s16)temp_s3->field_760 >= 0x24) {
                Gp_StateC08.field_6            |= 1;
                temp_s3->field_760              = 0;
                D_actor_403600_80160568.field_4 = 6;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 5:
        case 6:
            temp_v0_2          = temp_s3->field_760 + 1;
            temp_s3->field_760 = temp_v0_2;
            SOFT_TOUCH_REG(temp_v0_2);
            var_v0_2 = (s16)temp_v0_2 < 0x28;
            goto check_timeout;
    }
    goto end;

check_timeout:
    if (var_v0_2 != 0) {
        goto end;
    }
    temp_s3->field_760      = 0;
    D_actor_403600_8016056C = 0;
    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F1, 0, 0);

end:
    return;
}

void func_actor_403600_8013F608(Actor403600* arg0)
{
    Actor403600Work* work;
    s16              temp_v0_3;
    s16              temp_v1;
    s32              temp_arg2;
    s32              var_check;
    s32              var_s1;
    u16              temp_field;
    u16              temp_v0;
    u16              temp_v0_2;
    u16              temp_v0_4;
    u32              temp_t0;
    u32              temp_v0_5;

    work    = arg0->field_1C;
    temp_v1 = work->field_76C;
    if (temp_v1 == -1) {
        temp_v0         = (u16)work->field_768 + 1;
        work->field_768 = temp_v0;
        if ((s16)temp_v0 >= 3) {
            var_s1          = 1;
            work->field_768 = 0;
            do {
                Gp_SpawnEff(0x60080, (*Gp_ActorSlots)->extra->field_8 + var_s1, 0x400, NULL);
                var_s1 += 1;
            } while (var_s1 < 0x13);
        }
    } else {
        temp_v0_2       = (u16)work->field_768 + 1;
        work->field_768 = temp_v0_2;
        var_check       = (s16)temp_v0_2 < temp_v1;
        SOFT_TOUCH_REG_USE(work, var_check);
        temp_field = (u16)work->field_76C;
        if (!var_check) {
            temp_v0_3       = temp_field - 8;
            work->field_76C = temp_v0_3;
            if (temp_v0_3 < 3) {
                work->field_76C = 2;
            }
            temp_v0_4       = (u16)work->field_76A + 1;
            work->field_76A = temp_v0_4;
            if ((s16)temp_v0_4 >= 0x400) {
                work->field_76A = 0x400;
            }
            temp_v0_5   = (Gp_LcgState * 5) + 0x71357911;
            temp_t0     = temp_v0_5 >> 0x10;
            temp_arg2   = work->field_76A;
            Gp_LcgState = temp_v0_5;
            Gp_SpawnEff(0x60080,
                        (u8*)(*Gp_ActorSlots)->extra->field_8 + (((temp_t0 % 19) & 0xFFFF) * 0x50),
                        temp_arg2, NULL);
            work->field_768 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F7B8);

void func_actor_403600_8013FC2C(Actor403600Ctx* arg0, Actor403600* arg1)
{
    s16                       temp_a0;
    s16                       temp_v0_3;
    s16                       temp_v0_4;
    s16                       temp_v0_5;
    s32                       state;
    s32                       var_s0;
    u16                       temp_v0_6;
    u16                       temp_v0_8;
    u16                       temp_v0_9;
    VECTOR*                   temp_a1_2;
    MATRIX*                   temp_a1_5;
    register MATRIX*          matrix_head asm("s1");
    register MATRIX*          matrix_arg asm("a1");
    register MATRIX*          gte_block asm("s2");
    register SVECTOR*         rot_arg asm("a0");
    register TmdObject*       coord_object asm("v0");
    VECTOR*                   temp_a1_7;
    register GsCOORDINATE2*   temp_s0 asm("s0");
    u8*                       temp_s0_2;
    Actor403600Work*          temp_s1;
    Actor403600Work*          temp_s3;
    Actor403600Work*          temp_s4;
    TmdObject*                temp_s7;
    u8*                       temp_v0;
    u8*                       temp_v0_2;
    u8*                       restore1;
    u8*                       restore2;
    u8*                       var_s2;
    register GpEnemy*         color_actor1 asm("a0");
    register s32              color_zero1 asm("a2");
    register GpEnemy*         color_actor2 asm("a0");
    register s32              color_zero2 asm("a2");
    register Actor403600Work* color_work2 asm("v0");
    register s32              color_z1 asm("v0");
    register s32              color_arg3_1 asm("a3");
    register s32              color_arg3_2 asm("a3");

    temp_s7 = arg1->field_2C;
    temp_s4 = arg1->field_1C;
    state   = D_801153F4;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;

case1:
    color_actor1 = (GpEnemy*)arg0;
    actor_403600_load_scratch_head(temp_a1_2);
    temp_a1_2[-1].vx = temp_s4->field_4B8.workm.t[0];
    color_zero1      = 0;
    temp_a1_2        = (VECTOR*)((u8*)temp_a1_2 - 0x10);
    temp_a1_2->vy    = temp_s4->field_4B8.workm.t[1];
    actor_403600_color_tail(color_z1, color_arg3_1, temp_a1_2, temp_s4, color_zero1);
    temp_a1_2->vz = color_z1;
    Gp_UpdateActorColor(color_actor1, temp_a1_2, color_zero1, color_arg3_1);
    actor_403600_load_scratch_head_nop(restore1);
    restore1 += 0x10;
    actor_403600_store_scratch_head(restore1);
    goto end;

case2:
    temp_s7->field_C = 0x80;
    arg0->field_14   = 9;
    goto end;

default_body:
    func_actor_403600_80141C3C(arg1);
    func_actor_403600_8013DC7C(arg1);
    func_actor_403600_8013D15C(arg1);
    temp_s1 = arg1->field_1C;
    temp_a0 = temp_s1->field_736;
    if (D_actor_403600_8016057C[temp_a0] != 0) {
        var_s0 = 1;
        if (temp_a0 != temp_s1->field_738) {
            temp_s1->field_738 = (s16)(u16)temp_s1->field_736;
            temp_s1->field_73A = 0U;
            do {
                func_800B4114((GpAnimCtx*)temp_s1, var_s0, temp_s1->field_736, 0,
                              (s32)temp_s1->field_756);
                var_s0 += 1;
            } while (var_s0 < 0x14);
        } else {
            TOUCH_REG(var_s0);
            temp_s1->field_73A += var_s0;
            var_s2              = (u8*)temp_s1 + 0x28;
            do {
                var_s2[0x1D] = (u8)temp_s1->field_778;
                Gp_AnimTickIndex((GpAnimCtx*)temp_s1, var_s0);
                var_s0 += 1;
                var_s2 += 0x28;
            } while (var_s0 < 0x14);
        }
    }
    temp_s3 = arg1->field_1C;
    actor_403600_load_scratch_head(matrix_head);
    actor_403600_rot_setup(coord_object, rot_arg, matrix_arg, temp_s0, arg1, temp_s3, matrix_head);
    actor_403600_store_scratch_head(matrix_arg);
    gte_block = matrix_arg;
    RotMatrix(rot_arg, matrix_arg);
    temp_v0 = (u8*)temp_s0 + 0xA4;
    gte_SetRotMatrix(temp_v0);
    gte_ldclmv(gte_block);
    gte_rtir_real();
    gte_stclmv(temp_v0);
    gte_ldclmv((u8*)matrix_head - 0x1E);
    gte_rtir_real();
    temp_v0_2 = (u8*)temp_s0 + 0xA6;
    gte_stclmv(temp_v0_2);
    matrix_head = (MATRIX*)((u8*)matrix_head - 0x1C);
    gte_ldclmv(matrix_head);
    gte_rtir_real();
    temp_s0_2 = (u8*)temp_s0 + 0xA8;
    gte_stclmv(temp_s0_2);
    temp_v0_3 = temp_s3->field_700;
    if (temp_v0_3 != 0) {
        if (temp_v0_3 >= 0x20) {
            temp_v0_4          = (u16)temp_s3->field_700 - 0x20;
            temp_s3->field_700 = temp_v0_4;
            if ((temp_v0_4 << 0x10) <= 0) {
                temp_s3->field_700 = 0;
            }
        }
        if (temp_s3->field_700 < 0x21) {
            temp_v0_5          = (u16)temp_s3->field_700 + 0x20;
            temp_s3->field_700 = temp_v0_5;
            if ((temp_v0_5 << 0x10) >= 0) {
                temp_s3->field_700 = 0;
            }
        }
    }
    actor_403600_load_scratch_head(temp_a1_5);
    actor_403600_coord_advance(temp_a1_5, temp_s4);
    actor_403600_store_scratch_head(temp_a1_5);
    Gp_UpdateCoord(&temp_s4->field_4B8);
    temp_v0_6          = temp_s4->field_74C + 1;
    temp_s4->field_74C = temp_v0_6;
    if ((s16)temp_v0_6 >= 0xA) {
        color_actor2       = (GpEnemy*)arg0;
        temp_s4->field_74C = 0U;
        color_work2        = arg1->field_1C;
        actor_403600_load_scratch_head(temp_a1_7);
        temp_a1_7[-1].vx = color_work2->field_4B8.workm.t[0];
        color_zero2      = 0;
        temp_a1_7        = (VECTOR*)((u8*)temp_a1_7 - 0x10);
        temp_a1_7->vy    = color_work2->field_4B8.workm.t[1];
        actor_403600_color_tail_in_place(color_work2, color_arg3_2, temp_a1_7, color_zero2);
        temp_a1_7->vz = (s32)color_work2;
        Gp_UpdateActorColor(color_actor2, temp_a1_7, color_zero2, color_arg3_2);
        actor_403600_load_scratch_head_nop(restore2);
        restore2 += 0x10;
        actor_403600_store_scratch_head(restore2);
    }
    temp_v0_8          = temp_s4->field_74E + 1;
    temp_s4->field_74E = temp_v0_8;
    if ((((s16)temp_v0_8 % 42) << 0x10) == 0) {
        temp_v0_9          = temp_s4->field_750 + 1;
        temp_s4->field_750 = temp_v0_9;
        if ((s16)temp_v0_9 >= 0x64) {
            temp_s4->field_750 = 0x64U;
        }
    }
    if (((s16)temp_s4->field_74E >= temp_s4->field_754) || (temp_s4->field_742 != 0)) {
        arg0->field_14    = 1;
        temp_s7->field_2C = 0x12C;
        arg1->field_2A    = 0x3C;
        arg1->field_30    = (s32)(arg1->field_30 + 1);
    }

end:
    return;
}

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_801320A0);

void func_actor_403600_801400BC(Actor403600* arg0)
{
    u32              sp10;
    s32              sp14;
    s16              temp_v1;
    s32              temp_s2;
    s32              temp_v0;
    s32              temp_v1_2;
    s32              var_v0;
    s32              var_v0_2;
    s32              var_v0_3;
    s32              temp_s0_2;
    s32              temp_s0_4;
    u32              temp_v0_2;
    u32              temp_v0_3;
    GsCOORDINATE2*   temp_s0;
    GsCOORDINATE2*   temp_s0_3;
    Actor403600Work* temp_s1;

    temp_s1 = arg0->field_1C;
    temp_v1 = temp_s1->field_73E;
    switch (temp_v1) {
        case 0:
            temp_s1->field_736 = 1;
            temp_s1->field_73C = 0U;
            temp_s1->field_74A = 0;
            if (temp_s1->field_73A >= 0x1E) {
                temp_s1->field_73E = 1;
                temp_s1->field_73A = 0;
                return;
            }
        default:
            return;
        case 1:
            func_actor_403600_8013DDF4(arg0, 0);
            temp_s1->field_736 = 2;
            temp_s1->field_73C = temp_s1->field_750;
            temp_v1_2          = D_80073B8C->t[1];
            temp_v0            = temp_s1->field_4B8.coord.t[1] + 0x3E8;
            temp_s1->field_74A = (s16)((temp_v1_2 - temp_v0) / 25);
            func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
            if (sp10 < 0x835U) {
                var_v0 = sp14;
                if (var_v0 < 0) {
                    var_v0 = -var_v0;
                }
                if (var_v0 < 0x400) {
                    temp_s1->field_756 = 0;
                    temp_s1->field_73C = 0U;
                    temp_s1->field_73A = 0;
                    temp_s1->field_73E = 4;
                    return;
                }
            }
            break;
        case 3:
            temp_s1->field_588.field_18 = Gp_PackPair(&D_actor_403600_80150EB0, 0);
            temp_s1->field_736          = 0xC;
            temp_s1->field_73C          = 0U;
            temp_s1->field_74A          = 0;
            if (temp_s1->field_73A == 0xE) {
                temp_s0   = &temp_s1->field_4B8;
                temp_s2   = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x5416000D;
                temp_s0_2 = (s8)Gp_GetObjPan(temp_s0);
                temp_v0_2 = Gp_GetObjDepth(temp_s0);
                SndEvt_EnqueueType6(temp_s2, temp_s0_2,
                                    (s32)(((temp_v0_2 >> 0x1F) + temp_v0_2) << 0x17) >> 0x18);
            }
            if (temp_s1->field_73A == 0x11) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags | 0x8000);
            }
            if (temp_s1->field_73A == 0x15) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags & 0x7FFF);
            }
            if (temp_s1->field_73A >= 0x1E) {
                temp_s1->field_73C = 0U;
                temp_s1->field_73A = 0;
                func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_2 = sp14;
                    if (var_v0_2 < 0) {
                        var_v0_2 = -var_v0_2;
                    }
                    if (var_v0_2 < 0x400) {
                        temp_s1->field_73E = 4;
                        return;
                    }
                }
                goto block_33;
            }
            break;
        case 4:
            temp_s1->field_588.field_18 = Gp_PackPair(&D_actor_403600_80150EB0, 1);
            temp_s1->field_736          = 0xD;
            temp_s1->field_73C          = 0U;
            temp_s1->field_74A          = 0;
            if (temp_s1->field_73A == 9) {
                temp_s0_3 = &temp_s1->field_4B8;
                temp_s2   = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x5416000D;
                temp_s0_4 = (s8)Gp_GetObjPan(temp_s0_3);
                temp_v0_3 = Gp_GetObjDepth(temp_s0_3);
                SndEvt_EnqueueType6(temp_s2, temp_s0_4,
                                    (s32)(((temp_v0_3 >> 0x1F) + temp_v0_3) << 0x17) >> 0x18);
            }
            if (temp_s1->field_73A == 0xA) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags | 0x8000);
            }
            if (temp_s1->field_73A == 0xE) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags & 0x7FFF);
            }
            if (temp_s1->field_73A >= 0x23) {
                temp_s1->field_756 = 8;
                temp_s1->field_73C = 0U;
                temp_s1->field_73A = 0;
                func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_3 = sp14;
                    if (var_v0_3 < 0) {
                        var_v0_3 = -var_v0_3;
                    }
                    if (var_v0_3 < 0x400) {
                        temp_s1->field_73E = 3;
                        return;
                    }
                    goto block_33;
                }
                goto block_33;
            }
            break;
        block_33:
            temp_s1->field_73E = 0;
            return;
        case 5:
            temp_s1->field_736 = 9;
            temp_s1->field_73C = 0U;
            temp_s1->field_74A = 0;
            if (temp_s1->field_73A >= 0x46) {
                temp_s1->field_73E = 1;
                temp_s1->field_73A = 0;
                temp_s1->field_756 = 8;
            }
            break;
    }
}

void func_actor_403600_80140488(Actor403600Ctx* arg0, Actor403600* arg1)
{
    s32              state;
    s32              i;
    s16              countdown;
    TmdObject*       object;
    Actor403600Work* initialWork;
    Actor403600Work* globalWork;
    Actor403600Work* cleanupWork;
    Actor403600Work* commonWork;
    GpEnemy*         enemy;
    u8*              anim;

    object      = arg1->field_2C;
    initialWork = arg1->field_1C;
    globalWork  = ((Actor403600*)D_actor_403600_801606A8)->field_1C;
    state       = D_801153F4;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    if (globalWork->field_742 != 1) {
        return;
    }
    goto default_body;
case2:
    object->field_C |= 0x80;
    arg0->field_14   = 1;
    return;
default_body:
    if (initialWork->field_732 == 0) {
        goto inner0;
    }
    if (initialWork->field_732 == 1) {
        goto inner1;
    }
    goto common;
inner0:
    object->field_2C       += 3;
    arg1->field_2C->field_C = 0;
    countdown               = (u16)arg1->field_2A - 1;
    arg1->field_2A          = countdown;
    if ((countdown << 0x10) <= 0) {
        initialWork->field_732 = 1;
        initialWork->field_734 = 0;
    }
    goto common;
inner1:
    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x24);
    globalWork->field_4B4        = NULL;
    enemy                        = arg1->field_20;
    cleanupWork                  = arg1->field_1C;
    arg1->field_2C->field_8->sub = &Gfx_ViewCoord;
    enemy->field_54              = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&cleanupWork->field_508);
    Gp_UnlinkObj(&cleanupWork->field_588);
    if ((Task*)arg1 == D_actor_403600_801606A8) {
        Gp_UnlinkObj(&cleanupWork->field_5C0);
    }
    Gp_EnemyTaskExit((Task*)arg1);
    return;
common:
    commonWork = arg1->field_1C;
    if (D_actor_403600_8016057C[(s16)commonWork->field_736] != 0) {
        i = 1;
        if ((s16)commonWork->field_736 != commonWork->field_738) {
            commonWork->field_738 = commonWork->field_736;
            commonWork->field_73A = 0;
            do {
                func_800B4114((GpAnimCtx*)commonWork, i, commonWork->field_736, 0, commonWork->field_756);
                i++;
            } while (i < 0x14);
        } else {
            TOUCH_REG(i);
            commonWork->field_73A += i;
            anim                   = &commonWork->pad_0[0x28];
            do {
                anim[0x1D] = (u8)commonWork->field_778;
                Gp_AnimTickIndex((GpAnimCtx*)commonWork, i);
                i++;
                anim += sizeof(GpAnimSlot);
            } while (i < 0x14);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801406A4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80140B4C);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_801320EC);
