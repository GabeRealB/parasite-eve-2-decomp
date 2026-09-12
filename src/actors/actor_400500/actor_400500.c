#include "common.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];
extern u8    D_80072170;

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

extern TaskDesc D_actor_400500_80153D48;
extern u8       D_actor_400500_80153DD4[];

void func_8009EA50(s32 arg0);
void func_actor_400500_80132628(Task* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
s32  func_actor_400500_80132D74(Task* arg0);
s32  func_actor_400500_80133160(Task* arg0);
s32  func_actor_400500_80133358(Task* arg0);
s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_801335E8(Task* arg0);
void func_actor_400500_8013403C(Task* arg0);
void func_actor_400500_80139448(Task* arg0);
void func_actor_400500_8013A0B8(Task* arg0);
void func_actor_400500_8013C7A4(Task* arg0);
void func_actor_400500_8013CA38(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);
void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2);
void func_actor_400500_8013DC4C(Task* arg0);
void func_actor_400500_8013DCD4(Task* arg0);
s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);

void func_actor_400500_80132000(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->idMap;

    work->obj0.field_8  = &((TmdObject*)arg0->extra)->field_8[3];
    work->obj0.field_C  = work->rec0;
    work->obj0.field_14 = 0x110;
    work->obj0.field_10 = 0;
    work->obj0.field_12 = 0;
    work->obj0.field_18 = 0x30005;
    work->obj0.field_1C = 0x260;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(work->rec0, 3, 0);
    work->obj0.flags |= 0x8000;

    work->obj1.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj1.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj1.field_C  = work->rec1;
    work->obj1.field_10 = -0x460;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0;
    work->obj1.field_1C = 0x290;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags &= 0x7FFF;

    work->obj2.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj2.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj2.field_C  = work->rec1;
    work->obj2.field_10 = -0x200;
    work->obj2.field_12 = 0;
    work->obj2.field_14 = 0;
    work->obj2.field_1C = 0x250;
    work->obj2.flags    = 1;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj2.flags &= 0x7FFF;

    work->obj3.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj3.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj3.field_C  = work->rec2;
    work->obj3.field_10 = 0x460;
    work->obj3.field_12 = 0;
    work->obj3.field_14 = 0;
    work->obj3.field_1C = 0x290;
    work->obj3.flags    = 1;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj3.flags &= 0x7FFF;

    work->obj4.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj4.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj4.field_C  = work->rec2;
    work->obj4.field_10 = 0x200;
    work->obj4.field_12 = 0;
    work->obj4.field_14 = 0;
    work->obj4.field_1C = 0x250;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj4.flags &= 0x7FFF;
}

void func_actor_400500_8013226C(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    GsCOORDINATE2*     parts;
    GsCOORDINATE2*     part7;
    GsCOORDINATE2*     part10;
    GsCOORDINATE2*     coord;
    Task*              child;
    TmdObject*         extra;
    TmdObject*         tmd;
    TmdObject*         parentTmd;

    parts              = ((TmdObject*)arg0->extra)->field_8;
    work               = (Actor400500Work*)arg0->idMap;
    part7              = &parts[7];
    part10             = &parts[10];
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 0, 0, 0);
    work->field_9F0[0] = child;
    extra              = (TmdObject*)child->extra;
    coord              = extra->field_8;
    extra->field_C     = 0x80;
    coord->sub         = part10;
    coord->coord.t[0]  = 0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    src                = &rot;
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(-0x180, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    parentTmd     = (TmdObject*)arg0->extra;
    tmd           = (TmdObject*)child->extra;
    tmd->field_24 = parentTmd->field_24;
    tmd->field_25 = parentTmd->field_25;
    if (tmd->field_18 != NULL) {
        Tmd_ProcessStream(tmd);
        Tmd_ProcessStream(tmd);
    }
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 1, 0, 0);
    work->field_9F0[1] = child;
    extra              = (TmdObject*)child->extra;
    coord              = extra->field_8;
    extra->field_C     = 0x80;
    coord->sub         = part7;
    coord->coord.t[0]  = -0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    parentTmd          = (TmdObject*)arg0->extra;
    tmd                = (TmdObject*)child->extra;
    tmd->field_24      = parentTmd->field_24;
    tmd->field_25      = parentTmd->field_25;
    if (tmd->field_18 != NULL) {
        Tmd_ProcessStream(tmd);
        Tmd_ProcessStream(tmd);
    }
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(0x180, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132438);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132628);

void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    func_actor_400500_80132628(arg0, 3, 9, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 9, 0xA, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xA, 0xB, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 3, 6, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 6, 7, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 7, 8, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 5, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xC, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xC, 0xD, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xD, 0xE, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xF, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xF, 0x10, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0x10, 0x11, 0x100, (s32)arg1, temp_s2);
}

void func_actor_400500_80132C54(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    GsCOORDINATE2*     coord;
    s32                tx;

    work  = (Actor400500Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    switch (D_80072170) {
        case 1:
            tx                = 0x800;
            work->field_94A   = tx;
            work->field_94C   = tx;
            tx                = 0x14A0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 2:
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0x4074;
            work->field_94A   = 0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 3:
            tx                = 0xC00;
            work->field_94A   = tx;
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0xFA0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
    }
    tx                 = 0x1000;
    src                = &rot;
    rot.ident.m00_m01  = tx;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = tx;
    src->ident.m20_m21 = 0;
    src->ident.m22     = tx;
    RotMatrixZ(work->field_94C, &src->mat);
    func_8004BFF8(work->field_94A, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    func_actor_400500_8013DBCC(arg0, 0xB, &work->field_9A0);
}

s32 func_actor_400500_80132D74(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->idMap;
    if ((s16)work->field_A1C != 5) {
        if ((work->field_A16 < (0x500 - (work->field_9DC * 8))) &&
            ((u32)(work->field_A36 - 0x2E0) >= 0xA41U)) {
            if ((((u16)work->field_A32 >> 3) == 0) && !(work->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 1);
                return 1;
            }
            return 0;
        }
        if ((work->field_A16 < (0x640 - (work->field_9DC * 8))) &&
            ((u32)(work->field_A36 - 0x300) >= 0xA01U) &&
            (work->field_A32 == 0)) {
            if (!(((Actor400500Work*)arg0->idMap)->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 2);
            } else {
                func_actor_400500_8013DB64(arg0, 3);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132E94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133160);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133358);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133460);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801335E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133B14);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013403C);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013456C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801348D8);

void func_actor_400500_80134B88(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;

    D_800678F0[0] = D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)arg0->extra;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80134D6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135414);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", ActorsShared801328ccTable);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135770);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135EBC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801361EC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013662C);

void func_actor_400500_80136864(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->idMap;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801369A4);

void func_actor_400500_80136B94(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x400) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 1;
                return;
            }
            if ((s16)work->field_A1C == 4) {
                if (work->field_9E4 > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
            } else if (((0x400 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136D00(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            if (((u32)(work->field_A1C - 2) < 2U) || ((s16)work->field_A1C == 6)) {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 6;
                    return;
                }
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
            } else {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0xC00) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 3;
                    return;
                }
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136EB8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if (heading & 0xFFF) {
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                if (work->field_A1A != 3) {
                    if (work->field_A1A == 6) {
                        work->field_A08 = 7;
                    }
                } else if ((work->field_A34 == 0) && (work->field_9E4 < 0)) {
                    work->field_A08 = 7;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137034(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x800) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 8;
                return;
            }
            if ((s16)work->field_A1C == 6) {
                if (work->field_9E0 > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
            } else if (((0x800 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_801371A0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x800) {
                if (((0x800 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                switch (work->field_A1A) {
                    case 2:
                        if (coord->coord.t[2] < -0x209E) {
                            work->field_A08 = 9;
                        }
                        break;
                    case 3:
                        if ((work->field_A34 == 0) && (work->field_9E4 > 0)) {
                            work->field_A08 = 0xA;
                        }
                        break;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137338(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->idMap;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_80137478(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
            if (((0 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->idMap;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801375B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013771C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138088);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131EE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801385D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801387E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013899C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138B78);

void func_actor_400500_80138CE8(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Task*              child;
    GsCOORDINATE2*     coord;
    s32                angle;

    work                                = (Actor400500Work*)arg0->idMap;
    src                                 = &rot;
    angle                               = work->field_A26 - 0x80;
    work->field_A26                     = angle;
    child                               = ((Actor400500Work*)arg0->idMap)->field_9F0[0];
    ((TmdObject*)child->extra)->field_C = 0;
    coord                               = ((TmdObject*)child->extra)->field_8;
    rot.ident.m00_m01                   = 0x1000;
    rot.ident.m02_m10                   = 0;
    src->ident.m11_m12                  = 0x1000;
    rot.ident.m20_m21                   = 0;
    src->ident.m22                      = 0x1000;
    func_8004BFF8(-angle, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    if ((s16)work->field_A26 <= 0) {
        ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[0]->extra)->field_C = 0x80;
        work->field_A08                                                             = work->field_A08 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138DC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138EA0);

extern TaskFuncTable7 D_actor_400500_80131F2C;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F2C);

void func_actor_400500_8013905C(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable7         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131F2C;
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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801391B0);

void func_actor_400500_801392D8(Task* arg0)
{
    Actor400500Work*       work             = (Actor400500Work*)arg0->idMap;
    void                   (*fns[2])(Task*) = { func_actor_400500_8013C7A4, func_actor_400500_80139448 };
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500AnimStride* stride;
    s32                    i;

    fns[(s16)work->field_A08](arg0);
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
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A49 = 0;
    work4            = (Actor400500Work*)arg0->idMap;
    if (work4->field_A4A != 0) {
        work4->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
    }
}

void func_actor_400500_80139448(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    Actor400500Work* work5;
    s16              mode;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s32              flag;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A16 < 0x9C4) {
        Gp_ArmStateF0(1);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work2 = (Actor400500Work*)arg0->idMap;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 2;
        work3->field_A08 = 0;
        return;
    }
    mode = work->field_A3C;
    if (mode == 1) {
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
        pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work4 = (Actor400500Work*)arg0->idMap;
        if ((work4->field_A46 >= 0) || (((u8)work4->field_A46 & 0x7F) != mode)) {
            flag             = 0x81;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work5            = (Actor400500Work*)arg0->idMap;
        work5->field_A06 = 0;
        work5->field_A08 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801395D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013973C);

void func_actor_400500_80139AC4(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 flag;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        if ((s16)++work->field_A04 == 1) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050003;
            pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag == 0) {
            hit = (Actor400500HitView*)arg0->idMap;
            if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
                cond = 1;
            } else {
                cond = 0;
            }
            if (cond) {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_A06 = 0;
                work3->field_A08 = 0;
                work->field_A1E |= 1;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

extern TaskFuncTable3 D_actor_400500_80131F54;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F54);

void func_actor_400500_80139C1C(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable3         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131F54;
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
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A49 = 0;
}

void func_actor_400500_80139D70(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    GpEnemy*         enemy;
    u16              step;
    u16              accum;
    s32              y;
    s16              angle;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor400500Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)++work->field_A04 < 9) {
        if (enemy->field_40 <= 0) {
            work->field_A42 = 0;
            return;
        }
        work2 = (Actor400500Work*)arg0->idMap;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
        }
    } else {
        step              = (u16)work->field_A10 - 2;
        accum             = (u16)work->field_A12 + step;
        work->field_A12   = accum;
        work->field_A10   = step;
        y                 = coord->coord.t[1] - (s16)accum;
        coord->coord.t[1] = y;
        if (work->field_948 < 0x800) {
            angle           = (u16)work->field_948 + 0x98;
            work->field_948 = angle;
            if (angle >= 0x801) {
                work->field_948 = 0x800;
            }
        }
        if (coord->coord.t[1] < -0xFA0) {
            coord->coord.t[1] = -0xFA0;
            work->field_A08   = work->field_A08 + 1;
            coord->coord.t[1] = -0xFA0;
            work->field_948   = 0;
            work->field_94C   = 0x800;
            work->field_94A   = (u16)work->field_94A + 0x800;
            work2             = (Actor400500Work*)arg0->idMap;
            work2->field_9F8  = 0x10;
            work2->field_9FE  = 0x19;
            work2->field_9FA  = 2;
            soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
            pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
            pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
    }
}

void func_actor_400500_80139F6C(Task* arg0)
{
    Actor400500Work*       work             = (Actor400500Work*)arg0->idMap;
    void                   (*fns[2])(Task*) = { func_actor_400500_8013CA38, func_actor_400500_8013A0B8 };
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    s32                    i;

    fns[(s16)work->field_A08](arg0);
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
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A49 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A0B8);

extern TaskFuncTable7 D_actor_400500_80131F60;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F60);

void func_actor_400500_8013A484(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable7         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131F60;
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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A5D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A700);

void func_actor_400500_8013A8E4(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    GpEnemy*               enemy;
    s32                    mapped;
    s32                    i;

    work            = (Actor400500Work*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    mapped          = D_actor_400500_80153DD4[work->field_9FE];
    work->field_9F8 = 0x10;
    work->field_9FA = 2;
    work->field_9FE = mapped;
    work2           = (Actor400500Work*)arg0->idMap;
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
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj4);
    GameFlag_SetNibble(0xCE, 1);
    if (work->field_A40 == 4) {
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 6;
        work3->field_A08 = 0;
        return;
    }
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013AA98(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    Actor400500HitView*    hit;
    s32                    i;
    s32                    cond;

    work  = (Actor400500Work*)arg0->idMap;
    work2 = work;
    if (work->field_9FA == 1) {
        if ((s16)work->field_9FC != work->field_9FE) {
            work->field_A00 = 0;
        } else {
            work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work->field_9FA = 3;
        work->field_A00 = 0;
    } else if (work->field_9FA == 3) {
        work->field_A00 = (u16)work->field_A00 + 1;
    }
    i      = 1;
    stride = (Actor400500AnimStride*)work2 + 1;
    do {
        stride->field_1D = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
        stride++;
    } while (i < 0x12);
    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013ABE4(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    SVECTOR          pos;
    u16              frame;

    work  = (Actor400500Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    coord = model->field_8;

    work->field_A20 = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 4);
    work->field_A24 = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 4);
    work->field_A28 = (u16)work->field_A28 + (-work->field_A28 >> 4);
    model->field_2C = work->field_A24;
    func_8009EA50(work->field_A20);

    work->field_A02 = (u16)work->field_A02 - 0x30;
    scale.vx        = 0x1000;
    scale.vy        = work->field_A02;
    scale.vz        = 0x1000;
    coord->coord    = work->matrix_808;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;

    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0x10) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 5, &pos);
    }
    if ((s16)work->field_A04 >= 0x41) {
        model->field_C |= 0x80;
        work->field_A06 = work->field_A06 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AD60);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AF44);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B4A4);

extern TaskFuncTable5 D_actor_400500_80131FEC;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131FEC);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B720);
