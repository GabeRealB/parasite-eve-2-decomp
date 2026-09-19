#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"
#include "actors/actors_shared_8016a538.h"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where a
 * `SOFT_BARRIER()` was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

extern s32 Gp_LcgState;

/* Model streams in this overlay's own data. */
extern u8 D_actor_405800_8013FB18[];
extern u8 D_actor_405800_8014086C[];
extern u8 D_actor_405800_80140F10[];
extern u8 D_actor_405800_80141430[];

s32 func_actor_405800_801373E0(Task* arg0);

/* Defined in another unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_405800_80138FA8(Task* arg0);
void func_actor_405800_8013902C(Task* arg0);
void func_actor_405800_801390FC(Task* arg0);
void func_actor_405800_80139188(Task* arg0);
void func_actor_405800_80139260(Task* arg0);
void func_actor_405800_801392EC(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80136388);

s32 func_actor_405800_80136A1C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_858 == 1) {
        if (work->field_890 == 0) {
            switch (work->field_85A) {
                case 1:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 3;
                    work2->field_848 = 0;
                    return 1;
                case 2:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 3:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 5;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 5:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xF;
                    work2->field_848 = 0;
                    return 1;
            }
            return 0;
        } else {
            switch (work->field_85A) {
                case 1:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 3;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 2:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 3:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 5:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

s32 func_actor_405800_80136B94(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_890 == 0) {
        switch (work->field_85A) {
            case 1:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 3;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 2:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 3:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 5;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 4:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 5:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xF;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
        }
        work->field_85A = 0;
        return 0;
    } else {
        switch (work->field_85A) {
            case 1:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 3;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 2:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 3:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xE;
                work2->field_848 = 0;
                return 1;
            case 4:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 5:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xE;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
        }
        work->field_85A = 0;
        return 0;
    }
}

s32 func_actor_405800_80136CE0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_858 == 1) {
        if (work->field_890 == 0) {
            switch (work->field_85A) {
                case 1:
                    work->field_85A = 0;
                    return 0;
                case 2:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 3:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 5;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 5:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xF;
                    work2->field_848 = 0;
                    return 1;
            }
            work->field_85A = 0;
            return 0;
        } else {
            switch (work->field_85A) {
                case 1:
                    work->field_85A = 0;
                    return 0;
                case 2:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 3:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 5:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

void func_actor_405800_80136E14(Task* task)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    GpEffWork* eff4;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* dst4;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;
    TmdObject* src4;

    D_800678F0[0] = D_actor_405800_8013FB18;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[5], 0x200, NULL);
    if (eff != NULL) {
        src        = (TmdObject*)task->extra;
        dst        = (TmdObject*)eff->field_0->extra;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_405800_8014086C;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = (TmdObject*)task->extra;
        dst2        = (TmdObject*)eff2->field_0->extra;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_405800_80140F10;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = (TmdObject*)task->extra;
        dst3        = (TmdObject*)eff3->field_0->extra;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    D_800678F0[0] = D_actor_405800_80141430;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4        = (TmdObject*)task->extra;
        dst4        = (TmdObject*)eff4->field_0->extra;
        dst4->tpage = src4->tpage;
        dst4->clut  = src4->clut;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->coords)[3], 0x200, NULL);
}

void func_actor_405800_8013706C(Task* arg0, s16 arg1)
{
    Actor405800Work*        work = (Actor405800Work*)arg0->work;
    SVECTOR                 v;
    SVECTOR                 out;
    ActorsShared8016a538Mat rot;
    s16                     n;

    work->field_892 = arg1;
    switch (arg1) {
        case 0:
            if (work->field_890 == 0) {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.vx - ((TmdObject*)arg0->extra)->coords->coord.t[0];
                v.vy              = work->field_A8.vy - ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0x384;
                v.vz              = work->field_A8.vz - ((TmdObject*)arg0->extra)->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.vx - ((TmdObject*)arg0->extra)->coords->coord.t[0];
                v.vy              = work->field_A8.vy - ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0x640;
                v.vz              = work->field_A8.vz - ((TmdObject*)arg0->extra)->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                RotMatrixZ(-(s16)work->field_84, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->rec_744.end0.vx    = out.vx;
            work->rec_744.end0.vy    = out.vy;
            n                        = out.vz;
            work->rec_744.end0.vz    = n;
            n                        = 0xA;
            work->rec_744.end0Radius = n;
            work->rec_744.end1Radius = n;
            break;
        case 1:
            work->rec_744.end0.vx    = 0;
            work->rec_744.end0.vy    = 0x190;
            work->rec_744.end0.vz    = -0x1770;
            work->rec_744.end0Radius = 0x50;
            work->rec_744.end1Radius = 0x50;
            break;
    }
    work->rec_744.end1.vx = 0;
    work->rec_744.end1.vy = 0;
    work->rec_744.end1.vz = 0;
    Gp_ClearRec18Occupied(work->rec_75C);
    work->obj_724.flags |= 0x4000;
}

s32 func_actor_405800_8013728C(Task* arg0)
{
    Actor405800Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          v;
    s16              dist;
    s32              i;

    dist  = 0;
    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < 8; i++) {
        if ((work->rec_75C[i].key & 0xFFFF0000) != 0x100000) {
            dist = 0;
        } else {
            if (work->field_892 == 0) {
                dist = 1;
            } else if (work->field_892 == 1) {
                v.vx = work->rec_75C[i].point.vx - coord->workm.t[0];
                v.vy = 0;
                v.vz = work->rec_75C[i].point.vz - coord->workm.t[2];
                dist = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
                if (dist == 0) {
                    dist = 1;
                }
            } else if (work->field_892 == 2) {
                v.vx = work->rec_75C[i].point.vx - coord->workm.t[0];
                v.vy = work->rec_75C[i].point.vy - coord->workm.t[1];
                v.vz = 0;
                dist = SquareRoot0(v.vx * v.vx + v.vy * v.vy);
                if (dist == 0) {
                    dist = 1;
                }
            }
            break;
        }
    }
    Gp_ClearRec18Occupied(work->rec_75C);
    return dist;
}

s32 func_actor_405800_801373E0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    Actor405800Work* work4;
    Actor405800Work* work5;
    Actor405800Work* work6;
    Actor405800Work* work7;
    Actor405800Work* work8;
    GsCOORDINATE2*   coord;
    u32              rnd;
    u32              bits;
    s16              ang;
    s32              ret;

    rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
    bits        = rnd >> 0x10;
    Gp_LcgState = rnd;
    work        = (Actor405800Work*)arg0->work;
    coord       = ((TmdObject*)arg0->extra)->coords;
    if (work->field_85C == 0) {
        if (work->field_890 == 0) {
            if ((bits & 0xF) == 0) {
                if ((work->field_85E == 0) && (coord->coord.t[0] < 0x2710)) {
                    work->field_846 = 0xC;
                    work->field_848 = 0;
                    return 1;
                }
                return 0;
            }
            if ((u32)((bits & 7) - 1) < 3U) {
                if ((work->field_852 < 0x7D0) && ((u32)(work->field_856 - 0x200) >= 0xC01U) && ((u32)(work->field_854 - 0x601) < 0x3FFU)) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 8;
                    work2->field_848 = 0;
                    return ret;
                }
            } else if (work->field_852 < 0x640) {
                ang = (s16)work->field_856;
                if (ang < 0x400) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work3            = (Actor405800Work*)arg0->work;
                    work3->field_846 = 6;
                    work3->field_848 = 0;
                    return ret;
                }
                if (ang >= 0xC01) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work4            = (Actor405800Work*)arg0->work;
                    work4->field_846 = 7;
                    work4->field_848 = 0;
                    return ret;
                }
            }
        } else if ((bits & 7) == 0) {
            if (work->field_85E == 0) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work5            = (Actor405800Work*)arg0->work;
                work5->field_846 = 0x10;
                work5->field_848 = 0;
                return ret;
            }
            return 0;
        } else if ((bits & 0xF) == 1) {
            if (work->field_85E == 0) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work6            = (Actor405800Work*)arg0->work;
                work6->field_846 = 0xD;
                work6->field_848 = 0;
                return ret;
            }
            return 0;
        } else if (work->field_852 < 0x640) {
            ang = (s16)work->field_856;
            if (ang >= 0xC01) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work7            = (Actor405800Work*)arg0->work;
                work7->field_846 = 6;
                work7->field_848 = 0;
                return ret;
            }
            if (ang < 0x400) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work8            = (Actor405800Work*)arg0->work;
                work8->field_846 = 7;
                work8->field_848 = 0;
                return ret;
            }
            return 0;
        } else {
            return 0;
        }
    }
    return 0;
}

void func_actor_405800_801375C4(Task* arg0)
{
    Actor405800Work*         work;
    Task*                    child;
    TmdObject*               extra;
    ActorsShared8016a538Mat  rot;
    ActorsShared8016a538Mat* m;
    ActorsShared8016a538Mat* m2;
    s16                      angle;
    s16*                     p;

    work = (Actor405800Work*)arg0->work;
    if ((u8)work->field_88D != 0) {
        angle                             = (u16)work->field_87A + ((0x380 - work->field_87A) >> 2);
        work->field_87A                   = angle;
        child                             = ((Actor405800Work*)arg0->work)->field_828;
        ((TmdObject*)child->extra)->flags = 0;
        extra                             = (TmdObject*)child->extra;
        p                                 = (s16*)extra->coords;
        m                                 = &rot;
        rot.ident.m00_m01                 = 0x1000;
        rot.ident.m02_m10                 = 0;
        m->ident.m11_m12                  = 0x1000;
        rot.ident.m20_m21                 = 0;
        m->ident.m22                      = 0x1000;
        func_8004BFF8(angle, &m->mat);
        p[2] = rot.mat.m[0][0];
        p   += 2;
        p[1] = rot.mat.m[0][1];
        p[2] = rot.mat.m[0][2];
        p[3] = rot.mat.m[1][0];
        p[4] = rot.mat.m[1][1];
        p[5] = rot.mat.m[1][2];
        p[6] = rot.mat.m[2][0];
        p[7] = rot.mat.m[2][1];
        p[8] = rot.mat.m[2][2];
    } else {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
        angle                = (u16)work->field_87A + (-work->field_87A >> 3);
        work->field_87A      = angle;
        if (angle < 9) {
            ((TmdObject*)((Actor405800Work*)arg0->work)->field_828->extra)->flags = 0x80;
        } else {
            child                             = ((Actor405800Work*)arg0->work)->field_828;
            ((TmdObject*)child->extra)->flags = 0;
            extra                             = (TmdObject*)child->extra;
            p                                 = (s16*)extra->coords;
            m                                 = &rot;
            rot.ident.m00_m01                 = 0x1000;
            rot.ident.m02_m10                 = 0;
            m->ident.m11_m12                  = 0x1000;
            rot.ident.m20_m21                 = 0;
            m->ident.m22                      = 0x1000;
            func_8004BFF8(angle, &m->mat);
            p[2] = rot.mat.m[0][0];
            p   += 2;
            p[1] = rot.mat.m[0][1];
            p[2] = rot.mat.m[0][2];
            p[3] = rot.mat.m[1][0];
            p[4] = rot.mat.m[1][1];
            p[5] = rot.mat.m[1][2];
            p[6] = rot.mat.m[2][0];
            p[7] = rot.mat.m[2][1];
            p[8] = rot.mat.m[2][2];
        }
    }

    if ((u8)work->field_88E != 0) {
        angle                             = (u16)work->field_878 + ((0x380 - work->field_878) >> 2);
        work->field_878                   = angle;
        child                             = ((Actor405800Work*)arg0->work)->field_824;
        angle                             = -angle;
        ((TmdObject*)child->extra)->flags = 0;
        extra                             = (TmdObject*)child->extra;
        p                                 = (s16*)extra->coords;
        m                                 = &rot;
        rot.ident.m00_m01                 = 0x1000;
        rot.ident.m02_m10                 = 0;
        m->ident.m11_m12                  = 0x1000;
        rot.ident.m20_m21                 = 0;
        m->ident.m22                      = 0x1000;
        func_8004BFF8(angle, &m->mat);
        p[2] = rot.mat.m[0][0];
        p   += 2;
        p[1] = rot.mat.m[0][1];
        p[2] = rot.mat.m[0][2];
        p[3] = rot.mat.m[1][0];
        p[4] = rot.mat.m[1][1];
        p[5] = rot.mat.m[1][2];
        p[6] = rot.mat.m[2][0];
        p[7] = rot.mat.m[2][1];
        p[8] = rot.mat.m[2][2];
    } else {
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
        angle                = (u16)work->field_878 + (-work->field_878 >> 3);
        work->field_878      = angle;
        if (angle < 9) {
            ((TmdObject*)((Actor405800Work*)arg0->work)->field_824->extra)->flags = 0x80;
        } else {
            child                             = ((Actor405800Work*)arg0->work)->field_824;
            ((TmdObject*)child->extra)->flags = 0;
            extra                             = (TmdObject*)child->extra;
            p                                 = (s16*)extra->coords;
            m2                                = &rot;
            rot.ident.m00_m01                 = 0x1000;
            rot.ident.m02_m10                 = 0;
            m2->ident.m11_m12                 = 0x1000;
            rot.ident.m20_m21                 = 0;
            m2->ident.m22                     = 0x1000;
            func_8004BFF8((s16)-angle, &m2->mat);
            p[2] = rot.mat.m[0][0];
            p   += 2;
            p[1] = rot.mat.m[0][1];
            p[2] = rot.mat.m[0][2];
            p[3] = rot.mat.m[1][0];
            p[4] = rot.mat.m[1][1];
            p[5] = rot.mat.m[1][2];
            p[6] = rot.mat.m[2][0];
            p[7] = rot.mat.m[2][1];
            p[8] = rot.mat.m[2][2];
        }
    }

    if ((u32)(work->field_846 - 6) >= 2U) {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
    }
}

s32 func_actor_405800_80137908(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    if ((work->flags_83C.half & 1) || (work->flags_83C.word & 0x102)) {
        return 1;
    }
    return 0;
}

void func_actor_405800_80137948(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_88E = 0;
    work->field_88D = 0;
}

void func_actor_405800_8013795C(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (work->field_85C > 0) {
        work->field_85C = work->field_85C - 1;
    }
    if (work->field_85E > 0) {
        work->field_85E = work->field_85E - 1;
    }
}

void func_actor_405800_80137994(Task* arg0, s16 arg1)
{
    Actor405800Work* work;
    u32              rnd1;
    u32              rnd2;

    work = (Actor405800Work*)arg0->work;
    if ((arg1 << 16) != 0) {
        rnd1            = ((u32)Gp_LcgState * 5) + 0x71357911;
        rnd2            = (rnd1 * 5) + 0x71357911;
        Gp_LcgState     = rnd2;
        work->field_85C = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
        return;
    }
    work->field_85C = 0;
}

void func_actor_405800_801379F8(Task* task)
{
    Actor405800Work* work;

    work                = (Actor405800Work*)task->work;
    work->obj_724.flags = work->obj_724.flags & 0xBFFF;
}

void func_actor_405800_80137A14(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* cur;

    work = (Actor405800Work*)task->work;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    cur            = (Actor405800Work*)task->work;
    cur->field_846 = 1;
    cur->field_848 = 0;
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects unless
/// `func_actor_405800_80136A1C` or `func_actor_405800_801373E0` already
/// consumed the frame. After the handler, a set `field_890` plus a root
/// world X past 10000 switches to state 0xD. The two-entry table is small
/// enough that GCC materialises each callback with its own `lui`/`addiu`
/// pair instead of copying a `.rodata` pool.
void func_actor_405800_80137A60(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80138FA8, func_actor_405800_8013902C };

    func_actor_405800_80137948(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0 && (s16)func_actor_405800_801373E0(task) == 0) {
        states[(s16)work->field_848](task);
        if (work->field_890 != 0 && ((TmdObject*)task->extra)->coords->coord.t[0] > 10000) {
            Actor405800Work* cur = (Actor405800Work*)task->work;

            cur->field_846 = 0xD;
            cur->field_848 = 0;
        }
    }
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects. The
/// two-entry table is small enough that GCC materialises each callback with its
/// own `lui`/`addiu` pair instead of copying a `.rodata` pool.
void func_actor_405800_80137B34(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_801390FC, func_actor_405800_80139188 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80137B9C(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139260, func_actor_405800_801392EC };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}
