#include "common.h"

#include "main/gfx.h"
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

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

extern TaskDesc D_actor_400500_80153D48;

void func_8009EA50(s32 arg0);
void func_actor_400500_80132628(Task* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
s32  func_actor_400500_80132D74(Task* arg0);
s32  func_actor_400500_80133160(Task* arg0);
s32  func_actor_400500_80133358(Task* arg0);
s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132C54);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132D74);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136B94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136D00);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136EB8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137034);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801371A0);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013905C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801391B0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801392D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139448);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801395D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013973C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139C1C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139D70);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139F6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A0B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A484);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A5D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A700);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A8E4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AA98);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013ABE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AD60);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AF44);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B228);

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

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B5E0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B720);
