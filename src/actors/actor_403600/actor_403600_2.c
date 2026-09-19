#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/gfx.h"
#include "main/task.h"

#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtir_real()  __asm__ volatile("nop; nop; .word 0x4A49E012")
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
#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_403600_801320A0;
extern GpEnemyTaskFuncTable3 D_actor_403600_801320EC;
extern s32                   D_actor_403600_80160504[4];
extern s32                   D_actor_403600_8016057C[];
extern Task*                 D_actor_403600_801606A8;
extern s32                   D_80070F70;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1);
void func_actor_403600_80141338(Actor403600* arg0);
void func_actor_403600_801414FC(Actor403600* arg0);
void func_actor_403600_8014161C(Actor403600* arg0);
void func_actor_403600_80141A34(Actor403600* arg0);
void func_actor_403600_801400BC(Actor403600* arg0);
void func_actor_403600_80141F28(Actor403600* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

void func_actor_403600_801411D4(Actor403600* arg0, s32 arg1)
{
    Actor403600Work* work;
    s32              i;
    s32              masked1;
    s32              masked2;
    s32              limit1;
    s32              limit2;
    u8*              anim;

    work = arg0->field_1C;
    if (D_actor_403600_8016057C[(s16)work->field_736] != 0) {
        i = 1;
        if ((s16)work->field_736 != work->field_738) {
            masked1         = arg1 & 0xFF;
            work->field_738 = work->field_736;
            work->field_73A = 0;
            if (i < masked1) {
                limit1 = masked1;
                do {
                    func_800B4114((GpAnimCtx*)work, i, work->field_736, 0, work->field_756);
                    i++;
                } while (i < limit1);
            }
        } else {
            TOUCH_REG(i);
            masked2          = arg1 & 0xFF;
            work->field_73A += i;
            if (i < masked2) {
                limit2 = masked2;
                anim   = &work->pad_0[0x28];
                do {
                    anim[0x1D] = (u8)work->field_778;
                    Gp_AnimTickIndex((GpAnimCtx*)work, i);
                    i++;
                    anim += 0x28;
                } while (i < limit2);
            }
        }
    }
}

void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1)
{
    Actor403600Work* work;
    VECTOR*          head;
    VECTOR*          block;

    work                      = arg1->field_1C;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    head[-1].vx               = work->field_4B8.workm.t[0];
    block                     = (VECTOR*)((u8*)head - 0x10);
    block->vy                 = work->field_4B8.workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = work->field_4B8.workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0, block, 0, 0);
    *(VECTOR**)G_SCRATCH_HEAD = (VECTOR*)((u8*)*(VECTOR**)G_SCRATCH_HEAD + 0x10);
}

void func_actor_403600_80141338(Actor403600* arg0)
{
    Actor403600Work*      work;
    GsCOORDINATE2*        coord;
    MATRIX*               block;
    MATRIX*               head;
    s16                   value;
    s16                   decrement;
    s16                   increment;
    register Actor403600* actor asm("v1");
    register MATRIX*      matrixArg asm("a1");

    head = *(MATRIX**)G_SCRATCH_HEAD;
    SOFT_BARRIER();
    block                     = (MATRIX*)((u8*)head - 0x20);
    *(MATRIX**)G_SCRATCH_HEAD = block;
    matrixArg                 = block;
    actor                     = arg0;
    SOFT_USE_REG2(block, block);
    work  = actor->field_1C;
    coord = actor->field_2C->coords;
    RotMatrix((SVECTOR*)&work->field_700, matrixArg);

    gte_SetRotMatrix((u8*)coord + 0xA4);
    gte_ldclmv(block);
    gte_rtir_real();
    gte_stclmv((u8*)coord + 0xA4);

    gte_ldclmv((u8*)block + 2);
    gte_rtir_real();
    gte_stclmv((u8*)coord + 0xA6);

    gte_ldclmv((u8*)block + 4);
    gte_rtir_real();
    gte_stclmv((u8*)coord + 0xA8);

    value = work->field_700;
    if (value != 0) {
        if (value >= 0x20) {
            decrement       = (u16)work->field_700 - 0x20;
            work->field_700 = decrement;
            if ((decrement << 0x10) <= 0) {
                work->field_700 = 0;
            }
        }
        if (work->field_700 < 0x21) {
            increment       = (u16)work->field_700 + 0x20;
            work->field_700 = increment;
            if ((increment << 0x10) >= 0) {
                work->field_700 = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

void func_actor_403600_801414FC(Actor403600* arg0)
{
    Actor403600Work* work;
    s16              value;
    s16              countdown;
    s32              brightness;

    work  = arg0->field_1C;
    value = work->field_766;
    if (value != 0) {
        if (value < work->field_764) {
            brightness = rsin(D_80070F70 << 9) << 0xD;
        } else {
            brightness = rsin(D_80070F70 << 9) << 0xC;
        }
        Display_ClampField126((s8)(brightness >> 0x18));
        countdown       = (u16)work->field_764 - 1;
        work->field_764 = countdown;
        if ((countdown << 0x10) <= 0) {
            work->field_766 = 0;
            Display_ClampField126(0);
        }
    }
}

void func_actor_403600_80141598(Task* task)
{
    Actor403600Work* work;
    GpEnemy*         enemy;

    enemy                                  = task->spawnArg2;
    work                                   = (Actor403600Work*)task->work;
    ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
    enemy->field_54                        = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_508);
    Gp_UnlinkObj(&work->field_588);
    if (task == D_actor_403600_801606A8) {
        Gp_UnlinkObj(&work->field_5C0);
    }
    Gp_EnemyTaskExit(task);
}

void func_actor_403600_8014161C(Actor403600* arg0)
{
    Actor403600Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (*(s16*)((u8*)work + 0x78C) == 1) {
        if (((*(u16*)((u8*)work + 0x79E) & 3) == 3) &&
            ((work->field_73E != 0x28) || (work->field_732 != 5))) {
            Gp_SpawnEff(0x60055, arg0->field_2C->coords + 1, 0x12800, NULL);
        }
        *(u16*)((u8*)work + 0x79E)  = (u16)(*(u16*)((u8*)work + 0x79E) + 1);
        work->field_4B8.coord.t[1] += rsin(D_80070F70 << 8) >> 6;
        if (((s16) * (u16*)((u8*)work + 0x78A) / 10 < enemy->field_40) &&
            (*(u16*)((u8*)work + 0x79E) >= 0x385) && (work->field_73E == 0)) {
            work->field_730 = 6;
        }
    }
}

void func_actor_403600_8014174C(Actor403600* arg0)
{
    Actor403600Work* work;

    work            = arg0->field_1C;
    work->field_756 = 8;
    work->field_778 = 0x10;
    work->field_776 = 0xA;
    work->field_742 = 0;
    work->field_746 = 0;
    work->field_774 = 0;
    work->field_77A = 0;
    work->field_784 = 0;
    work->field_73C = 0;
    work->field_73E = 0;
    work->field_74A = 0;
    work->field_73A = 0;
    work->field_76E = 0x40;
    work->field_75E = 0;
    work->field_7A4 = 0;
    work->field_7A6 = 0;
    work->field_7AC = 0;
}

void func_actor_403600_801417A8(Actor403600* arg0, s32 arg1)
{
    SVECTOR          rotation;
    MATRIX*          matrix;
    Actor403600Work* work;
    s16              angle;
    s32              absAngle;
    u16              wrapped;

    work  = arg0->field_1C;
    angle = work->field_75E + (arg1 & 0xFF);
    SOFT_TOUCH_REG(angle);
    SOFT_TOUCH_REG(angle);
    absAngle = angle;
    if (angle < 0) {
        SOFT_TOUCH_REG(absAngle);
        absAngle = -absAngle;
    }
    work->field_75E = angle;
    if (absAngle >= 0x801) {
        wrapped = angle - 0x1000;
        if (angle <= 0) {
            wrapped = 0x1000 - angle;
        }
        work->field_75E = wrapped;
    }
    matrix = &work->field_4B8.coord;
    Gfx_MatrixToEuler(matrix, &rotation);
    rotation.vz += work->field_75E;
    RotMatrix(&rotation, matrix);
}

s32 func_actor_403600_80141840(Actor403600* arg0)
{
    s32              targetY;
    s32              currentY;
    s32              count;
    s32              deltaX;
    s32              deltaY;
    s32              deltaZ;
    register s32     left asm("v0");
    register s32     right asm("v1");
    s16              amount;
    Actor403600Work* work;

    count = 0;
    left  = 1;
    SOFT_BARRIER();
    work            = arg0->field_1C;
    work->field_746 = left;
    func_actor_403600_8013DDF4(arg0, 0xA0);

    left   = work->field_6F0.vx;
    right  = work->field_4B8.coord.t[0];
    amount = 0x12C;
    deltaX = left - right;
    SOFT_BARRIER();
    if (deltaX < 0) {
        deltaX = -deltaX;
    }
    work->field_73C = amount;
    if (deltaX < 0x1F5) {
        count                      = 1;
        work->field_4B8.coord.t[0] = work->field_6B0.vx;
    }

    targetY  = work->field_6F0.vy;
    currentY = work->field_4B8.coord.t[1];
    deltaY   = targetY - currentY;
    if (deltaY < 0) {
        deltaY = -deltaY;
    }
    if (deltaY < 0x1F5) {
        count                     += 1;
        work->field_4B8.coord.t[1] = work->field_6B0.vy;
    } else if (targetY < currentY) {
        *(s16*)((u8*)work + 0x74A)  = -0x12C;
        work->field_4B8.coord.t[1] += rsin(D_80070F70 << 8) >> 6;
    } else {
        work->field_74A = amount;
    }

    left   = work->field_6F0.vz;
    right  = work->field_4B8.coord.t[2];
    deltaZ = left - right;
    if (deltaZ < 0) {
        deltaZ = -deltaZ;
    }
    if (deltaZ < 0x1F5) {
        work->field_4B8.coord.t[2] = work->field_6B0.vz;
        count                     += 1;
    }
    return count & 0xFF;
}

void func_actor_403600_80141954(s32 arg0)
{
    RECT rect;

    rect.y = 0x80;
    rect.h = 0x80;
    rect.w = 0x80;
    if (arg0 == 1) {
        rect.x = 0x180;
    } else {
        rect.x = 0x1C0;
    }
    MoveImage(&rect, 0x180, 0x180);
    rect.w = 0x100;
    rect.h = 1;
    rect.x = 0;
    if (arg0 == 1) {
        rect.y = 0xFD;
    } else {
        rect.y = 0xFE;
    }
    MoveImage(&rect, 0, 0xF9);
}

void func_actor_403600_801419E8(Actor403600* arg0)
{
    TmdObject* obj;

    obj               = arg0->field_2C;
    *(s8*)&obj->tpage = -0xF;
    obj->clut         = 2;
    if (obj->buffer != NULL) {
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
    }
}

void func_actor_403600_80141A34(Actor403600* arg0)
{
    RECT             rect;
    Actor403600Work* work;
    s16              value;

    work  = arg0->field_1C;
    value = work->field_784;
    if (work->field_7A8 != value) {
        if (value == 1) {
            s16 width;

            rect.x = 0x162;
            rect.y = 0x162;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x16F;
            rect.y = 0x102;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        } else {
            s16 width;

            rect.x = 0x141;
            rect.y = 0x1F3;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x17F;
            rect.y = 0x1AB;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        }
        work->field_7A8 = (u16)work->field_784;
    }
}

void func_actor_403600_80141B24(Actor403600* arg0)
{
    Actor403600Work* work = arg0->field_1C;

    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x54160001, 1);
    work->field_708 = 0;
}

void func_actor_403600_80141B60(Actor403600* arg0)
{
    s16              nextCountdown;
    u16              countdown;
    u16              currentMp;
    Actor403600Work* work;
    PlayerStatus*    config;

    work            = arg0->field_1C;
    countdown       = (u16)work->field_792 - 1;
    work->field_792 = countdown;
    if ((countdown << 0x10) <= 0) {
        config     = &Player_Status;
        currentMp  = config->mp + 1;
        config->mp = currentMp;
        if ((s16)currentMp >= config->mpMax) {
            config->mp = config->mpMax;
        }
        if (work->field_794 <= 0) {
            work->field_792 = 1;
            return;
        }
        nextCountdown   = (u16)work->field_794 - 1;
        work->field_794 = nextCountdown;
        work->field_792 = nextCountdown;
    }
}

void func_actor_403600_80141BE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320A0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403600_80141C3C(Actor403600* arg0)
{
    s16 value;

    value = arg0->field_1C->field_730;
    if (value < 0) {
        return;
    }
    if (value < 2) {
        func_actor_403600_801400BC(arg0);
    }
}

void func_actor_403600_80141C7C(Actor403600* arg0, s32 arg1)
{
    GpEnemy*         enemy;
    Actor403600Work* work;

    enemy           = arg0->field_20;
    work            = arg0->field_1C;
    enemy->field_40 = *(u16*)&enemy->field_40 - arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->field_40 <= 0) {
        work->field_742 = 1;
    }
}

void func_actor_403600_80141CD4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320EC;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403600_80141D30(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   workCoord;
    GsCOORDINATE2*   coord;
    Actor403600Work* work;
    MATRIX*          matrix;
    MATRIX*          matrix2;

    coord = ((TmdObject*)arg1->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work                 = (TaskIdMap*)work;
    work->field_4B8.sub        = &gGfxViewCoord;
    matrix                     = &work->field_4B8.coord;
    *(s32*)&matrix->m[0][0]    = 0x1000;
    *(s32*)&matrix->m[0][2]    = 0;
    *(s32*)&matrix->m[1][1]    = 0x1000;
    *(s32*)&matrix->m[2][0]    = 0;
    matrix->m[2][2]            = 0x1000;
    work->field_4B8.coord.t[0] = coord->coord.t[0];
    work->field_4B8.coord.t[1] = coord->coord.t[1];
    workCoord                  = &work->field_4B8;
    work->field_4B8.coord.t[2] = coord->coord.t[2];
    matrix2                    = &coord->coord;
    coord->sub                 = workCoord;
    *(s32*)&matrix2->m[0][0]   = 0x1000;
    *(s32*)&matrix2->m[0][2]   = 0;
    *(s32*)&matrix2->m[1][1]   = 0x1000;
    *(s32*)&matrix2->m[2][0]   = 0;
    matrix2->m[2][2]           = 0x1000;
    coord->coord.t[1]          = 0x690;
    coord->coord.t[0]          = 0;
    coord->coord.t[2]          = 0x5DC;
    work->field_4B8.flg        = 0;
    Gp_UpdateCoord(workCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    work->field_4B8.coord.t[0] = 0;
    work->field_4B8.coord.t[1] = 0;
    work->field_4B8.coord.t[2] = 0;
    work->field_730            = 0;
    arg1->msgTable             = D_actor_403600_80160504;
    arg1->exitCallback         = (TaskFunc)func_actor_403600_80141F28;
    work->field_77A            = 0x2328;
    work->field_744            = 0;
    arg1->state               += 1;
}

void func_actor_403600_80141E78(Actor403600Ctx* arg0, Actor403600* arg1)
{
    TmdObject*       obj;
    TmdObject*       obj2;
    Actor403600Work* work;
    u16              value;

    work            = arg1->field_1C;
    value           = work->field_744 + 1;
    work->field_744 = value;
    if ((s16)value >= 2) {
        Tmd_AllocBuffers(arg1->field_2C);
        obj          = arg1->field_2C;
        obj->flags  &= 0xFFFB;
        obj2         = arg1->field_2C;
        obj2->flags &= 0xFF7F;
        arg1->field_30++;
        work->field_73A = 0;
        work->field_77C = 0;
        work->field_744 = 0x1000;
    }
}

void func_actor_403600_80141F28(Actor403600* arg0)
{
    arg0->field_2C->coords->sub = &gGfxViewCoord;
    Gp_EnemyTaskExit((Task*)arg0);
}

void func_actor_403600_80141F58(GpAnimMtxRec* arg0, s32 arg1)
{
    void**   scratch;
    void*    head;
    SVECTOR* vec;
    MATRIX*  matrix;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (SVECTOR*)((u8*)head - 8);
    *scratch = vec;
    matrix   = &arg0->mtx;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 0, 6, 12);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 2, 8, 14);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 4, 10, 16);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 4, 10, 16);

    head          = *scratch;
    arg0->field_0 = 0;
    *scratch      = (u8*)head + 8;
}
