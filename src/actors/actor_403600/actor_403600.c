#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "psyq/inline_c.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define actor_403600_load_scratch_head(out) \
    __asm__ volatile("lui %0, 0x1F80; lw %0, 0x3FC(%0)" : "=r"(out))
#define actor_403600_store_scratch_head(value) \
    __asm__ volatile("lui $1, 0x1F80; sw %0, 0x3FC($1)" : : "r"(value) : "$1", "memory")
#define actor_403600_restore_scratch_head(value) \
    __asm__("addiu %0, %0, 24; lui $1, 0x1F80; sw %0, 0x3FC($1)" : "+r"(value) : : "$1")

extern u8       D_80071075;
extern u8       D_801153F4;
extern MATRIX*  D_80073B8C;
extern s32      D_8005C374;
extern s32      D_8007107C;
extern s16      D_80073BA0;
extern u32      Gp_LcgState;
extern u8       D_actor_403600_80150ED4;
extern TaskDesc D_actor_403600_801421A0;
extern s32      D_actor_403600_8016056C;
extern s32      D_actor_403600_8016057C[];
extern Actor403600Point D_actor_403600_801605F4[];
extern s32      D_actor_403600_80160698;
extern s32      D_actor_403600_8016069C;
extern s32      D_actor_403600_801606A0;
extern Task*    D_actor_403600_801606A8;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
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

    var_a2 = (Actor403600Work*)arg0->parent->idMap;
    if (arg0->state == 0) {
        temp_v0 = Mem_Calloc(0x11C, false);
        if (temp_v0 == NULL) {
            Task_CallExit(arg0);
            return;
        }
        Game_Session->field_80 = 0;
        arg0->idMap            = temp_v0;
        temp_v0_2              = Task_SpawnFromTable(&D_actor_403600_801421A0, 2, 0, 0);
        if (temp_v0_2 != NULL) {
            Task_Reparent(arg0, temp_v0_2);
        }
        var_a2                  = (Actor403600Work*)arg0->parent->idMap;
        var_a2->field_710       = arg0;
        D_actor_403600_801606A0 = 0;
        arg0->state            += 1;
        goto block_6;
    }
block_6:
    temp_a3                 = arg0->idMap;
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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80135C28);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136224);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136500);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013685C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136C00);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013700C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80137300);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801375F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801379B4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138004);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801386EC);

void func_actor_403600_80138C34(Task* arg0)
{
    Task* parent;

    parent = arg0->parent;
    func_actor_403600_80132E40(parent, (Actor403600Work*)parent->parent->idMap, (Actor403600Work*)parent->idMap);
}

void func_actor_403600_80138C68(Task* arg0)
{
    Gp_UnlinkObj(&((Actor403600Work*)arg0->idMap)->obj);
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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", ActorsShared80131e24Sub0);

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
        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[0].x;
        temp_v1               = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[0].z;
        temp_s3->z            = temp_v1;
        temp_v0               = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0 * temp_v0) + (temp_v1 * temp_v1));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[1].x;
        temp_v1_2             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[1].z;
        temp_s3->z            = temp_v1_2;
        temp_v0_2             = temp_s3->x;
        temp_s3->distances[1] = SquareRoot0((temp_v0_2 * temp_v0_2) + (temp_v1_2 * temp_v1_2));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[2].x;
        temp_v1_3             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[2].z;
        temp_s3->z            = temp_v1_3;
        temp_v0_3             = temp_s3->x;
        temp_s3->distances[2] = SquareRoot0((temp_v0_3 * temp_v0_3) + (temp_v1_3 * temp_v1_3));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[3].x;
        temp_v1_4             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[3].z;
        temp_s3->z            = temp_v1_4;
        temp_v0_4             = temp_s3->x;
        temp_s3->distances[3] = SquareRoot0((temp_v0_4 * temp_v0_4) + (temp_v1_4 * temp_v1_4));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[4].x;
        temp_v1_5             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[4].z;
        temp_s3->z            = temp_v1_5;
        temp_v0_5             = temp_s3->x;
        temp_s3->distances[4] = SquareRoot0((temp_v0_5 * temp_v0_5) + (temp_v1_5 * temp_v1_5));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[5].x;
        temp_v1_6             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[5].z;
        temp_s3->z            = temp_v1_6;
        temp_v0_6             = temp_s3->x;
        temp_s3->distances[5] = SquareRoot0((temp_v0_6 * temp_v0_6) + (temp_v1_6 * temp_v1_6));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[6].x;
        temp_v1_7             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[6].z;
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
        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[7].x;
        temp_v1_8             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[7].z;
        temp_s3->z            = temp_v1_8;
        temp_v0_8             = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0_8 * temp_v0_8) + (temp_v1_8 * temp_v1_8));

        temp_s3->x            = Wip_SysConfig.field_4->t[0] - D_actor_403600_801605F4[8].x;
        temp_v1_9             = Wip_SysConfig.field_4->t[2] - D_actor_403600_801605F4[8].z;
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
        Game_Session->field_1   = 1;
        D_actor_403600_8016056C = 0;
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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DFE0);

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
    x                            = Wip_SysConfig.field_4->t[0] - arg0->coord.t[0];
    *(s32*)((s8*)scratch + 0x20) = x;
    *(s32*)((s8*)scratch + 0x24) = Wip_SysConfig.field_4->t[1] - arg0->coord.t[1];
    z                            = Wip_SysConfig.field_4->t[2] - arg0->coord.t[2];
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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F0C0);

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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013FC2C);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_801320A0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801400BC);

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
