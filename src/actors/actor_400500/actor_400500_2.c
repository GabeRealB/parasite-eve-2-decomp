#include "common.h"
#include "psyq/inline_c.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"

s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_8013403C(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);
void func_actor_400500_8013DC4C(Task* arg0);
void func_actor_400500_8013DCD4(Task* arg0);
s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);

void func_actor_400500_8013AF44(Task* arg0)
{
    MATRIX                 local;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coords;
    Actor400500AnimStride* stride;
    Actor400500ViewPos*    pos;
    Actor400500ViewPos*    pos2;
    s32                    flag;
    s32                    heading;
    s32                    i;
    u16                    a1c;

    work    = (Actor400500Work*)arg0->idMap;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = ((TmdObject*)arg0->extra)->field_8;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0)) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 4;
        work2->field_9FA = 2;
        work3            = (Actor400500Work*)arg0->idMap;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i      = 1;
        stride = (Actor400500AnimStride*)work3 + 1;
        do {
            stride->field_1D = (u8)work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
            stride++;
        } while (i < 0x12);
        switch ((s16)((u16)work->field_A1A - 1)) {
            case 3:
                if (heading != 0x400) {
                    work->field_A08 = 4;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 0:
                if (work->field_9E0 >= 0) {
                    work->field_A08 = 3;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 1:
                a1c = work->field_A1C;
                if ((u32)(a1c - 1) < 2U) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 4) || ((s16)work->field_A1C == 5)) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 3) && (heading == 0)) {
                    if (coord->coord.t[0] >= 0x4074) {
                        work->field_A08 = 6;
                    } else {
                        work->field_A08 = 1;
                    }
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 2:
                if (work->field_9E4 < 0) {
                    work->field_A08 = 6;
                } else {
                    work->field_A08 = 8;
                }
                break;
            case 5:
                if (heading == 0x800) {
                    work->field_A08 = 8;
                } else {
                    work->field_A08 = 7;
                }
                break;
            default:
                coord->coord.t[0] = -0x3E8;
                coord->coord.t[1] = -0xFA0;
                coord->coord.t[2] = -0x2116;
                work->field_94A   = 0x400;
                work->field_A08   = 1;
                break;
        }
        pos2   = &work->field_9A0;
        coords = ((TmdObject*)arg0->extra)->field_8;
        Gp_UpdateCoord(&coords[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[8].workm, &local);
        pos           = pos2;
        pos->x        = local.t[0];
        pos->z        = local.t[2];
        coords[8].flg = 0;
    }
}

void func_actor_400500_8013B228(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              a1a;
    u16              a1c;

    work  = (Actor400500Work*)arg0->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0x400) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            a1a = work->field_A1A;
            if (a1a != 1) {
                if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                    a1c = work->field_A1C;
                    if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                        work->field_A08 = 5;
                    } else {
                        work->field_A08 = a1a;
                    }
                } else {
                    func_actor_400500_8013403C(arg0);
                }
            } else {
                if (work->field_9E0 < -0xF9F) {
                    work3            = (Actor400500Work*)arg0->idMap;
                    work3->field_A06 = 9;
                    work3->field_A08 = 0;
                }
                func_actor_400500_8013403C(arg0);
            }
        }
        coord->coord.t[2] = -0x209E;
    }
}

void func_actor_400500_8013B374(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              a1c;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        a1c = work->field_A1C;
        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
            if (!((u16)work->field_94A & 0xFFF)) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 4;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
        } else if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work3            = (Actor400500Work*)arg0->idMap;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 4;
            work3->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work4            = (Actor400500Work*)arg0->idMap;
        work4->field_A06 = 9;
        work4->field_A08 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013B4A4);

extern TaskFuncTable5 D_actor_400500_80131FEC;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500_2", D_actor_400500_80131FEC);

void func_actor_400500_8013B5E0(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable5         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131FEC;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->idMap;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i      = 1;
    stride = (Actor400500AnimStride*)work2 + 1;
    do {
        stride->field_1D = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
        stride++;
    } while (i < 0x12);
}

s32 func_actor_400500_8013B720(GsCOORDINATE2* arg0, MATRIX* arg1)
{
    MATRIX         matrix;
    MATRIX         parent;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;
    MATRIX*        parentp;

    coord = arg0->sub;
    if (coord == &Gfx_ViewCoord) {
        return 0;
    }
    view    = &Gfx_ViewCoord;
    parentp = &parent;
    matrix  = coord->coord;
    while (1) {
        coord = coord->sub;
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            break;
        }
        parent = coord->coord;
        MatrixNormal(parentp, parentp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    __asm__ volatile(
        "lhu $12, 0(%0);"
        "lhu $13, 6(%0);"
        "lhu $14, 12(%0);"
        "sh $12, 0(%1);"
        "sh $13, 2(%1);"
        "sh $14, 4(%1);"
        "lhu $12, 2(%0);"
        "lhu $13, 8(%0);"
        "lhu $14, 14(%0);"
        "sh $12, 6(%1);"
        "sh $13, 8(%1);"
        "sh $14, 10(%1);"
        "lhu $12, 4(%0);"
        "lhu $13, 10(%0);"
        "lhu $14, 16(%0);"
        "sh $12, 12(%1);"
        "sh $13, 14(%1);"
        "sh $14, 16(%1);"
        : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}
