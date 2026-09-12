#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
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
extern s32      D_8005C374;
extern s32      D_8007107C;
extern s16      D_80073BA0;
extern TaskDesc D_actor_403600_801421A0;
extern s32      D_actor_403600_8016056C;
extern s32      D_actor_403600_8016057C[];
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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801327A0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013289C);

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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138C9C);

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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013955C);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_80131E34);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801396F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013A444);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013C864);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013CCEC);

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

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DC7C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DDF4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DFE0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E470);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E66C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E7D4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013EA04);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F0C0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F608);

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
