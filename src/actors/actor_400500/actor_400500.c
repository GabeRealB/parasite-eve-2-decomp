#include "common.h"
#include "psyq/inline_c.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"
#include "actors/coord_to_view.h"

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

extern GpPairSrcE D_actor_400500_80153C90;
extern u8         D_actor_400500_80153CA0[];
extern u8         D_actor_400500_80153CC0[];
extern TaskDesc   D_actor_400500_80153D48;
extern u16        D_actor_400500_80153DB4[];
extern u8         D_actor_400500_80153DD4[];
extern s32        Gp_LcgState;

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
void func_actor_400500_8013B4A4(Task* arg0);
void func_actor_400500_8013C7A4(Task* arg0);
void func_actor_400500_8013CA38(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);
void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2);
void func_actor_400500_8013DC4C(Task* arg0);
void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400500_8013DCD4(Task* arg0);
s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DDEC(Task* arg0);
void func_actor_400500_8013DF50(Task* arg0);

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

void func_actor_400500_80132438(Task* arg0)
{
    SVECTOR            dir;
    SVECTOR*           dirp;
    SVECTOR            delta;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Actor400500Work*   work;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     other;
    s16                dist;
    s16                heading;
    s16                vz;
    s32                y;
    s32                z;
    s32                one;
    u16                counter;

    work  = (Actor400500Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_ActorSlots[0] != NULL) {
        other              = Gp_ActorSlots[0]->extra->field_8;
        work->field_9C0.vx = coord->coord.t[0];
        work->field_9C0.vy = coord->coord.t[1];
        work->field_9C0.vz = coord->coord.t[2];
        if ((s16)work->field_A1C != 5) {
            dir.vx = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            dir.vy = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            dir.vz = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        } else {
            work->field_A32 = 2;
            counter         = work->field_A22 + 1;
            work->field_A22 = counter;
            if (!(counter & 0x100)) {
                dir.vx = 0x2710 - (u16)coord->coord.t[0];
            } else {
                dir.vx = 0x3E8 - (u16)coord->coord.t[0];
            }
            y      = -0x3E8;
            dir.vy = y - (u16)coord->coord.t[1];
            z      = -0x20D0;
            dir.vz = z - (u16)coord->coord.t[2];
        }
        dist = SquareRoot0((dir.vx * dir.vx) + (dir.vz * dir.vz));
        do {
            work->field_9E0 = (u16)dir.vx;
            dirp            = &dir;
            work->field_9E2 = (u16)dir.vy;
        } while (0);
        vz              = (u16)dir.vz;
        work->field_A16 = dist;
        work->field_9E4 = vz;
        VectorNormalSS(dirp, dirp);
        work->field_A36    = (ratan2(dir.vx, dir.vz) - (u16)work->field_94A) & 0xFFF;
        delta.vx           = (u16)other->coord.t[0] - (u16)work->field_9D0.vx;
        delta.vy           = (u16)other->coord.t[1] - (u16)work->field_9D0.vy;
        one                = 0x1000;
        delta.vz           = (u16)other->coord.t[2] - (u16)work->field_9D0.vz;
        src                = &rot;
        rot.ident.m00_m01  = one;
        rot.ident.m02_m10  = 0;
        src->ident.m11_m12 = one;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = one;
        heading            = work->field_94A;
        ((void (*)(s32, MATRIX*))func_8004BFF8)(-heading, &src->mat);
        ApplyMatrixSV(&src->mat, &delta, &work->field_9D8);
    }
}

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
        if ((work->field_A16 < (0x500 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x2E0) >= 0xA41U)) {
            if ((((u16)work->field_A32 >> 3) == 0) && !(work->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 1);
                return 1;
            }
            return 0;
        }
        if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) &&
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

void func_actor_400500_80132E94(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TmdObject*       extra;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->idMap;
    extra = (TmdObject*)arg0->extra;
    if (work->field_A46 < 0) {
        if (!((u8)work->field_A46 & 1)) {
            switch (work->field_A47) {
                case 0:
                    work->field_A20 = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 2);
                    if (work->field_A20 >= 0xF8) {
                        work->field_A20 = 0xFF;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    func_8009EA50(work->field_A20);
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2C < work->field_A2A) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A24 = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 2);
                    work->field_A28 = (u16)work->field_A28 + (-work->field_A28 >> 2);
                    if (work->field_A24 == 0) {
                        enemy->node.field_4 = 1;
                        if ((u8)work->field_A4C == 0) {
                            enemy->node.field_4 = 5;
                        }
                        work->field_A28 = 0;
                        work->field_A46 = 0;
                        extra->field_C |= 0x80;
                    }
                    extra->field_2C = work->field_A24;
                    break;
            }
        } else {
            switch (work->field_A47) {
                case 0:
                    enemy->node.field_4 = 0;
                    if ((u8)work->field_A4C == 0) {
                        enemy->node.field_4 = 4;
                    }
                    extra->field_C &= ~0x80;
                    work->field_A24 = (u16)work->field_A24 + ((s16)(0x1000 - (u16)work->field_A24) >> 2);
                    work->field_A28 = (u16)work->field_A28 + ((0xFF - work->field_A28) >> 2);
                    if (work->field_A24 >= 0xFF0) {
                        work->field_A28 = 0xFF;
                        work->field_A24 = 0x1000;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    extra->field_2C = work->field_A24;
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2A >= 0x11) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A20 = (u16)work->field_A20 + ((s16) - (u16)work->field_A20 >> 2);
                    if (work->field_A20 < 9) {
                        work->field_A20 = 0;
                        work->field_A46 = 0;
                        func_actor_400500_8013B4A4(arg0);
                        if (work->field_A30 == 0) {
                            work->field_A30 = (u16)work->field_A2E;
                        }
                    }
                    func_8009EA50(work->field_A20);
                    break;
            }
        }
    }
    if (work->field_A30 > 0) {
        work->field_A30 = (u16)work->field_A30 - 1;
    }
}

s32 func_actor_400500_80133160(Task* arg0)
{
    Actor400500Work* work;
    s32              soundId;
    s32              pan;
    u16              heading;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A38 == 1) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x17, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050001;
            pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        heading         = (u16)work->field_94A - D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    if (work->field_A38 == 2) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x18, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050002;
            pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        heading         = (u16)work->field_94A + D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133358(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;
    s32                 ret;

    work = (Actor400500Work*)arg0->idMap;
    mode = work->field_A3C;
    if (mode == 1) {
        ret = 0;
        sub = work->field_A3E;
        if (sub == mode) {
            goto zero_both;
        }
        if ((sub == 2) || (sub == 4)) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_A0E = 4;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xA;
            work2->field_9FA = 1;
            work->field_A3E  = 0;
            goto check_hit;
        }
        if (sub != 3) {
            goto check_hit;
        }
        func_actor_400500_8013DB64(arg0, 0xC);
        ret = 1;
    zero_both:
        work->field_A3C = 0;
        work->field_A3E = 0;
        return ret;
    check_hit:
        hit = (Actor400500HitView*)arg0->idMap;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133460(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;

    work = (Actor400500Work*)arg0->idMap;
    mode = work->field_A3C;
    if (mode == 1) {
        sub = work->field_A3E;
        if (sub == mode) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x1C;
            work2->field_9FE = 0xB;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 2) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 4) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 3) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xE;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        }
        hit = (Actor400500HitView*)arg0->idMap;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801335E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133B14);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013403C);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013456C);

void func_actor_400500_801348D8(Task* arg0, s32 arg1)
{
    SVECTOR                pos;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         joint;
    GsCOORDINATE2*         player;
    GpActorWork*           slot;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500AnimStride* stride;
    Actor400500AnimStride* stride2;
    s32                    i;
    s32                    cur;
    s32                    sample;

    coords = ((TmdObject*)arg0->extra)->field_8;
    slot   = *Gp_ActorSlots;
    joint  = coords + 8;
    work   = (Actor400500Work*)arg0->idMap;
    if (slot != NULL) {
        player = slot->extra->field_8;
        work2  = work;
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
        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        joint->flg = 0;
        Gp_UpdateCoord(joint);
        pos.vx = 0x160;
        pos.vy = 0x148;
        pos.vz = 0x2C0;
        ActorCoordToView(joint, &pos);
        if ((arg1 << 0x10) == 0) {
            player->coord.t[0] = pos.vx;
            player->coord.t[2] = pos.vz;
        } else {
            sample             = pos.vx;
            cur                = player->coord.t[0];
            cur               += (sample - cur) >> 2;
            player->coord.t[0] = cur;
            sample             = pos.vz;
            cur                = player->coord.t[2];
            cur               += (sample - cur) >> 2;
            player->coord.t[2] = cur;
        }
        player->flg = 0;
        Gp_UpdateCoord(player);
        work->field_9F8 = -0x10;
        work3           = (Actor400500Work*)arg0->idMap;
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
        i       = 1;
        stride2 = (Actor400500AnimStride*)work3 + 1;
        do {
            stride2->field_1D = (u8)work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
            stride2++;
        } while (i < 0x12);
        work->field_9F8 = 0x10;
    }
}

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

void func_actor_400500_80135414(Task* arg0)
{
    TmdObject*             extra;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500Work*       work5;
    Actor400500Work*       work6;
    Actor400500Work*       work7;
    Actor400500AnimStride* stride;
    GsCOORDINATE2*         player;
    GsCOORDINATE2*         coord2;
    TmdObject*             extra2;
    s32                    i;
    s32                    flag;
    s32                    val;
    u8                     mode;

    extra       = arg0->extra;
    enemy       = arg0->spawnArg2;
    coord       = extra->field_8;
    arg0->idMap = Mem_Calloc(0xA50, 0);
    work        = (Actor400500Work*)arg0->idMap;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    extra->field_1C    = &work->lightMtx;
    extra->field_20    = &work->colorMtx;
    extra->field_C     = 0;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)arg0->extra)->field_8[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_54     = (s32)work->rec0;
    enemy->field_50     = &D_actor_400500_80153C90;
    enemy->field_40 = enemy->field_42 = D_actor_400500_80153C90.field_4;
    func_800B3F84(&work->anim, D_actor_400500_80153CC0, (GpAnimObj*)extra, work->pad_2E4,
                  work->slots);
    coord->sub       = &Gfx_ViewCoord;
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_9F8 = 0x18;
    work2->field_9FE = 2;
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
    arg0->field_24 = D_actor_400500_80153CA0;
    func_actor_400500_80132C54(arg0);
    work4 = (Actor400500Work*)arg0->idMap;
    if (Gp_ActorSlots[0] != NULL) {
        player              = Gp_ActorSlots[0]->extra->field_8;
        work4->field_9D0.vx = (u16)player->coord.t[0];
        work4->field_9D0.vy = (u16)player->coord.t[1];
        work4->field_9D0.vz = (u16)player->coord.t[2];
    }
    work5  = (Actor400500Work*)arg0->idMap;
    mode   = Game_Session->field_5;
    extra2 = arg0->extra;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        val              = 0xFF;
        work5->field_A20 = val;
        val              = 0x10;
        work5->field_A24 = 0;
        work5->field_A28 = 0;
    } else {
        val              = 0x1000;
        work5->field_A24 = val;
        val              = 0xFF;
        work5->field_A28 = val;
        val              = 0x2000;
        work5->field_A20 = 0;
    }
    work5->field_A2C = val;
    SOFT_BARRIER();
    func_8009EA50(work5->field_A20);
    extra2->field_2C = work5->field_A24;
    func_actor_400500_80132000(arg0);
    func_actor_400500_8013226C(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    coord2                = ((TmdObject*)arg0->extra)->field_8;
    work->eff_940.field_4 = 0x100;
    work->eff_940.field_6 = 3;
    work->eff_940.field_0 = &coord2[3];
    work6                 = (Actor400500Work*)arg0->idMap;
    work6->field_A06      = 6;
    work6->field_A08      = 0;
    work7                 = (Actor400500Work*)arg0->idMap;
    D_80062735            = 2;
    if (((work7->field_A46 >= 0) || ((u8)work7->field_A46 & 0x7F)) && (work7->field_A30 == 0)) {
        flag             = 0x80;
        work7->field_A46 = flag;
        work7->field_A47 = 0;
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", ActorsShared801328ccTable);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135770);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135EBC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801361EC);

void func_actor_400500_8013662C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;
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
            if ((heading & 0xFFF) != 0x400) {
                if (((0x400 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
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
                        func_actor_400500_801335E8(arg0);
                    }
                } else {
                    if (work->field_A34 == 0) {
                        if (work->field_9E0 <= 0) {
                            work->field_A08 = 2;
                        }
                    } else if (work->field_9E0 < -0xF9F) {
                        rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 0x10) & 0x1F) == 0) {
                            if (!(work->field_A1E & 1)) {
                                work2 = (Actor400500Work*)arg0->idMap;
                                val   = 7;
                            } else {
                                work2 = (Actor400500Work*)arg0->idMap;
                                val   = 8;
                            }
                            work2->field_A06 = val;
                            work2->field_A08 = 0;
                        }
                    }
                    func_actor_400500_801335E8(arg0);
                }
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

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

void func_actor_400500_801369A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;

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
            if ((heading & 0xFFF) != 0xC00) {
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->idMap;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if (a1a == 4) {
                        work->field_A08 = a1a;
                    }
                } else if (work->field_A34 == 0) {
                    if (work->field_9E0 >= 0) {
                        work->field_A08 = 4;
                    }
                } else if (work->field_9E0 >= 0xFA0) {
                    rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = rnd;
                    if (((rnd >> 0x10) & 0x1F) == 0) {
                        if (!(work->field_A1E & 1)) {
                            work2 = (Actor400500Work*)arg0->idMap;
                            val   = 7;
                        } else {
                            work2 = (Actor400500Work*)arg0->idMap;
                            val   = 8;
                        }
                        work2->field_A06 = val;
                        work2->field_A08 = 0;
                    }
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

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

void func_actor_400500_801375B8(Task* arg0)
{
    Actor400500Work*       work;
    Actor400500Work*       work2;
    Actor400500Work*       work3;
    Actor400500Work*       work4;
    Actor400500AnimStride* stride;
    s32                    flag;
    s32                    i;

    work                = (Actor400500Work*)arg0->idMap;
    work->obj0.field_1C = 0x130;
    work2               = (Actor400500Work*)arg0->idMap;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 5;
    work3->field_9FA = 2;
    work4            = (Actor400500Work*)arg0->idMap;
    if (work4->field_9FA == 1) {
        if ((s16)work4->field_9FC != work4->field_9FE) {
            work4->field_A00 = 0;
        } else {
            work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work4->field_9FA = 3;
    } else if (work4->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work4->field_9FA = 3;
        work4->field_A00 = 0;
    } else if (work4->field_9FA == 3) {
        work4->field_A00 = (u16)work4->field_A00 + 1;
    }
    i      = 1;
    stride = (Actor400500AnimStride*)work4 + 1;
    do {
        stride->field_1D = (u8)work4->field_9F8;
        Gp_AnimTickIndex(&work4->anim, i);
        i++;
        stride++;
    } while (i < 0x12);
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_9BC = 0;
    work->field_A08 = work->field_A08 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013771C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138088);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131EE4);

extern TaskFuncTable3 D_actor_400500_80131EF0;
extern TaskFuncTable3 D_actor_400500_80131EFC;

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_400500_80131EF0\n"
        "dlabel D_actor_400500_80131EF0\n"
        "    .word func_actor_400500_8013BE50\n"
        "    .word func_actor_400500_8013BEC4\n"
        "    .word func_actor_400500_801387E8\n"
        "enddlabel D_actor_400500_80131EF0\n"
        "nonmatching D_actor_400500_80131EFC\n"
        "dlabel D_actor_400500_80131EFC\n"
        "    .word func_actor_400500_8013BC9C\n"
        "    .word func_actor_400500_8013BCCC\n"
        "    .word func_actor_400500_8013BD64\n"
        "enddlabel D_actor_400500_80131EFC\n"
        ".section .text");
#endif

void func_actor_400500_801385D0(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable3         sp10;
    TaskFuncTable3         sp20;
    Actor400500Work*       workA;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    skip;
    s32                    i;

    work  = (Actor400500Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp10  = D_actor_400500_80131EF0;
    sp20  = D_actor_400500_80131EFC;
    if (enemy->field_40 <= 0) {
        if (work->field_A40 == 4) {
            work->field_A42 = 0;
        } else {
            sp20.funcs[(s16)work->field_A0A](arg0);
        }
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        goto common;
    }
    workA = (Actor400500Work*)arg0->idMap;
    if (workA->field_A4A != 0) {
        workA->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        skip = 1;
    } else {
        skip = 0;
    }
    if (skip == 0) {
        sp10.funcs[(s16)work->field_A08](arg0);
        ((Actor400500Work*)arg0->idMap)->field_A49 = 0;
        func_actor_400500_80133358(arg0);
    common:
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
}

void func_actor_400500_801387E8(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    Actor400500Matrix   rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    s32                 flag;
    u16                 frame;

    work            = (Actor400500Work*)arg0->idMap;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xB) {
        work->obj1.flags |= 0x8000;
        work->obj2.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_A04 >= 0x12) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[1]->extra)->field_C = 0x80;
            work->obj1.flags                                                           &= 0x7FFF;
            work->obj2.flags                                                           &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
        work3            = (Actor400500Work*)arg0->idMap;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

extern TaskFuncTable5 D_actor_400500_80131F08;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F08);

void func_actor_400500_8013899C(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable5         sp;
    Actor400500Work*       workA;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;
    s32                    skip;

    work  = (Actor400500Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F08;
    if (enemy->field_40 > 0) {
        workA = (Actor400500Work*)arg0->idMap;
        if (workA->field_A4A != 0) {
            workA->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            skip = 1;
        } else {
            skip = 0;
        }
        if (skip == 0) {
            sp.funcs[(s16)work->field_A08](arg0);
            goto common;
        }
    } else {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
    common:
        func_actor_400500_80133358(arg0);
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
        work3->field_A49 = 0;
    }
}

void func_actor_400500_80138B78(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Matrix   rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    u16                 frame;

    work            = (Actor400500Work*)arg0->idMap;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xD) {
        work->obj3.flags |= 0x8000;
        work->obj4.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_A04 >= 0x10) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[0]->extra)->field_C = 0x80;
            work->obj3.flags                                                           &= 0x7FFF;
            work->obj4.flags                                                           &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A08 = work->field_A08 + 1;
    }
}

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

void func_actor_400500_80138DC4(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    s32                 cond;
    s32                 flag;
    u32                 rnd;

    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work = (Actor400500Work*)arg0->idMap;
        if (((work->field_A46 >= 0) || ((u8)work->field_A46 & 0x7F)) && (work->field_A30 == 0)) {
            flag            = 0x80;
            work->field_A46 = flag;
            work->field_A47 = 0;
        }
        ((Actor400500Work*)hit)->field_A32 = 0x3C;
        rnd                                = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState                        = rnd;
        if (!((rnd >> 0x10) & 3)) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_A06 = 0;
            work2->field_A08 = 0;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 8;
        work3->field_A08 = 0;
    }
}

extern TaskFuncTable4 D_actor_400500_80131F1C;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F1C);

void func_actor_400500_80138EA0(Task* arg0)
{
    Actor400500Work*       work;
    GpEnemy*               enemy;
    TaskFuncTable4         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work  = (Actor400500Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F1C;
    if ((enemy->field_40 <= 0) && (work->field_A40 == 4)) {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        return;
    }
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

void func_actor_400500_801391B0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->idMap;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[1]->extra)->field_C = 0x80;
    work->obj1.flags                                                           &= 0x7FFF;
    work->obj2.flags                                                           &= 0x7FFF;
    ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[0]->extra)->field_C = 0x80;
    work->obj3.flags                                                           &= 0x7FFF;
    flags                                                                       = work->field_A1E;
    work->obj4.flags                                                           &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->idMap;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

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

extern TaskFuncTable3 D_actor_400500_80131F48;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131F48);

void func_actor_400500_801395D0(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable3         sp;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131F48;
    if ((s16)work->field_A08 != 0) {
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
    sp.funcs[(s16)work->field_A08](arg0);
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->idMap;
    work3->field_A49 = 0;
}

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

void func_actor_400500_8013A5D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->idMap;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[1]->extra)->field_C = 0x80;
    work->obj1.flags                                                           &= 0x7FFF;
    work->obj2.flags                                                           &= 0x7FFF;
    ((TmdObject*)((Actor400500Work*)arg0->idMap)->field_9F0[0]->extra)->field_C = 0x80;
    work->obj3.flags                                                           &= 0x7FFF;
    flags                                                                       = work->field_A1E;
    work->obj4.flags                                                           &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->idMap;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

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

extern TaskFuncTable11 D_actor_400500_80131FA4;

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131FA4);

void func_actor_400500_8013AD60(Task* arg0)
{
    Actor400500Work*       work;
    TaskFuncTable11        sp;
    GpEnemy*               enemy;
    Actor400500Work*       work2;
    Actor400500AnimStride* stride;
    s32                    i;
    Actor400500Work*       work3;
    s32                    flag;

    work  = (Actor400500Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131FA4;
    if ((s16)work->field_A08 != 0) {
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
    if (enemy->field_40 > 0) {
        sp.funcs[(s16)work->field_A08](arg0);
    } else {
        work->field_A42 = 0;
    }
    work3 = (Actor400500Work*)arg0->idMap;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}
