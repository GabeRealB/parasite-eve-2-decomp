#include "common.h"

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

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

void func_8009EA50(s32 arg0);
s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_8013403C(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);

extern TaskFuncTable3 D_actor_400500_80131EE4;

void func_actor_400500_8013BA24(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    TaskFuncTable3   sp;

    work = (Actor400500Work*)arg0->idMap;
    sp   = D_actor_400500_80131EE4;
    sp.funcs[(s16)work->field_A08](arg0);
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_A3C = 0;
    work2->field_A3E = 0;
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_A49 = 0;
}

void func_actor_400500_8013BAA4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A40 == 4) {
        work->field_A42 = 0;
        return;
    }
    if (!(work->field_A1E & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 1;
        work2->field_9FA = 2;
        work->field_A04  = 0;
        work->field_A18  = 0;
        work->field_A10  = 0;
        work->field_A12  = 0;
        work->field_A0A  = work->field_A0A + 1;
        return;
    }
    work->field_A42 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013BB18);

void func_actor_400500_8013BBB0(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->idMap;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013BC9C(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A42 = 1;
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_A10 = 0;
    work->field_A12 = 0;
    work->field_A0A = work->field_A0A + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013BCCC);

void func_actor_400500_8013BD64(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->idMap;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->idMap;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013BE50);

void func_actor_400500_8013BEC4(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Task*              child;
    GsCOORDINATE2*     coord;
    Actor400500Work*   work2;
    s32                angle;

    work                                = (Actor400500Work*)arg0->idMap;
    src                                 = &rot;
    work->field_A26                     = work->field_A26 + 0x80;
    work->field_A04                     = work->field_A04 + 1;
    child                               = ((Actor400500Work*)arg0->idMap)->field_9F0[1];
    angle                               = work->field_A26;
    ((TmdObject*)child->extra)->field_C = 0;
    coord                               = ((TmdObject*)child->extra)->field_8;
    rot.ident.m00_m01                   = 0x1000;
    rot.ident.m02_m10                   = 0;
    src->ident.m11_m12                  = 0x1000;
    rot.ident.m20_m21                   = 0;
    src->ident.m22                      = 0x1000;
    func_8004BFF8(angle, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 8;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013BFB0);

void func_actor_400500_8013C018(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Matrix  rot;
    Actor400500Matrix* src;
    Task*              child;
    GsCOORDINATE2*     coord;
    Actor400500Work*   work2;
    s32                angle;

    work                                = (Actor400500Work*)arg0->idMap;
    src                                 = &rot;
    work->field_A26                     = work->field_A26 + 0x80;
    work->field_A04                     = work->field_A04 + 1;
    child                               = ((Actor400500Work*)arg0->idMap)->field_9F0[0];
    angle                               = work->field_A26;
    ((TmdObject*)child->extra)->field_C = 0;
    coord                               = ((TmdObject*)child->extra)->field_8;
    rot.ident.m00_m01                   = 0x1000;
    rot.ident.m02_m10                   = 0;
    src->ident.m11_m12                  = 0x1000;
    rot.ident.m20_m21                   = 0;
    src->ident.m22                      = 0x1000;
    func_8004BFF8(-angle, &src->mat);
    ActorsShared80132c4c(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 7;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C108);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C174);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C218);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C348);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C3C4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C474);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C508);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C578);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C61C);

void func_actor_400500_8013C750(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->idMap;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013C7A4);

void func_actor_400500_8013C818(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Work*   work2;
    Actor400500RootXZ* coord;
    s32                soundId;
    s32                pan;

    coord   = (Actor400500RootXZ*)((TmdObject*)arg0->extra)->field_8;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
    work    = (Actor400500Work*)arg0->idMap;
    pan     = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x20;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0x80;
    work->field_A12  = 0;
    work->field_A04  = 0;
    work->field_A08  = work->field_A08 + 1;
    work->field_950  = coord->x;
    work->field_954  = coord->z;
}

void func_actor_400500_8013C908(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work    = (Actor400500Work*)arg0->idMap;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x15;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0;
    work->field_A12  = 0x12C;
    work->field_948  = 0;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013C9D4(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->idMap;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2                                = (Actor400500Work*)arg0->idMap;
        work2->field_A06                     = 0;
        work2->field_A08                     = 0;
        ((Actor400500Work*)work)->field_A1E &= 0xFFFE;
    }
}

void func_actor_400500_8013CA38(Task* arg0)
{
    MATRIX              local;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    GsCOORDINATE2*      coords;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    s32                 heading;
    s32                 masked;
    s32                 neg;

    work             = (Actor400500Work*)arg0->idMap;
    heading          = (u16)work->field_94A;
    work->field_A04  = 0;
    work2            = (Actor400500Work*)arg0->idMap;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x16;
    work2->field_9FA = 2;
    masked           = heading & 0xFFF;
    if ((work->field_A1A == 1) && ((masked == 0x400) || (masked == 0xC00))) {
        neg             = -1;
        work->field_A04 = neg;
        pos2            = &work->field_9A0;
        coords          = ((TmdObject*)arg0->extra)->field_8;
        Gp_UpdateCoord(&coords[0xE]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[0xE].workm, &local);
        pos             = pos2;
        pos->x          = local.t[0];
        pos->z          = local.t[2];
        coords[0xE].flg = 0;
    }
    work->field_A08 = work->field_A08 + 1;
}

void func_actor_400500_8013CB0C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

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
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013CBD8);

void func_actor_400500_8013CCDC(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

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
        if (((u16)work->field_94A & 0xFFF) == 0x400) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 1;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013CDA8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;
    s32              flag;

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
        if ((u16)work->field_94A & 0xFFF) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            if (work->field_A1A != 3) {
                if (work->field_A1A == 6) {
                    work->field_A08 = 7;
                }
            } else if (work->field_9E4 > 0) {
                work->field_A08 = 7;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

void func_actor_400500_8013CE9C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

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
        if (((u16)work->field_94A & 0xFFF) == 0x800) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 8;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013CF68);

void func_actor_400500_8013D078(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

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
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013D144(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

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
        if (((u16)work->field_94A & 0xFFF) == 0) {
            work2            = (Actor400500Work*)arg0->idMap;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 6;
            return;
        }
        work3            = (Actor400500Work*)arg0->idMap;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013D210(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;

    work              = (Actor400500Work*)arg0->idMap;
    coord             = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work->field_94C   = 0x800;
    work->field_94A   = 0;
    coord->coord.t[0] = 0x4074;
    coord->coord.t[1] = -0xFA0;
    coord->coord.t[2] = -0x2710;
    work->field_A04   = 0;
    work2             = (Actor400500Work*)arg0->idMap;
    work2->field_9F8  = 4;
    work2->field_9FE  = 1;
    work2->field_9FA  = 2;
    work->field_A08   = work->field_A08 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D274);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D2D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D3B8);

void func_actor_400500_8013D420(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work = (Actor400500Work*)arg0->idMap;
    if ((s16)++work->field_A04 == 0x1E) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40050004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((u8)work->field_A4B == 4) {
        work->field_A4C  = 0;
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
    }
}

void func_actor_400500_8013D4F0(Task* arg0)
{
    GpEnemy*         enemy;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        if (Gp_TickObjFlag2((GpObj5D*)enemy) != 0) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D59C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D630);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D6A0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013D744);

void func_actor_400500_8013D878(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->idMap;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

void func_actor_400500_8013D8CC(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;

    model            = (TmdObject*)arg0->extra;
    work             = (Actor400500Work*)arg0->idMap;
    coord            = model->field_8;
    work->field_A02  = 0x1000;
    work->matrix_808 = coord->coord;
    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013D958(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400500Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x18) {
        work->field_A20 = 0;
        work->field_A24 = 0x1000;
        work->field_A28 = 0xFF;
        func_8009EA50(work->field_A20);
        model->field_2C = work->field_A24;
        work->field_A04 = 0;
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013D9DC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013D9F4(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model           = (TmdObject*)arg0->extra;
    work            = (Actor400500Work*)arg0->idMap;
    model->field_C |= 0x80;
    work->field_A04 = 0;
    work->field_A28 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DA24(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->idMap;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 2) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013DA68(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model = (TmdObject*)arg0->extra;
    work  = (Actor400500Work*)arg0->idMap;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    func_actor_400500_80134B88(arg0);
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DACC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013DAE4(Task* arg0, s32 arg1, u16* arg2)
{
    Actor400500Work* work;
    s32              kind;

    work = (Actor400500Work*)arg0->idMap;
    kind = arg2[1];
    switch (kind) {
        case 1:
            work->field_A4C = kind;
            work->field_A2C = 0x1E;
            work->field_A4B = kind;
            func_actor_400500_8013DB64(arg0, 0xB);
            break;
        case 2:
            work->field_A4B = kind;
            break;
        case 3:
            work->field_A4B = kind;
            break;
        case 4:
            work->field_A4B = kind;
            break;
    }
}

void func_actor_400500_8013DB64(Task* arg0, s16 arg1)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A06 = arg1;
    work->field_A08 = 0;
}

s32 func_actor_400500_8013DB78(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    if ((work->field_A16 < (0x640 - (work->field_9DC * 8))) && ((u32)(work->field_A36 - 0x300) >= 0xA01U)) {
        work->field_A06 = 9;
        work->field_A08 = 0;
        return 1;
    }
    return 0;
}

void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2)
{
    MATRIX         local;
    GsCOORDINATE2* coord;

    coord = &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    arg2->x    = local.t[0];
    arg2->z    = local.t[2];
    coord->flg = 0;
}

void func_actor_400500_8013DC4C(Task* arg0)
{
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)arg0->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = (u8)work->field_9F8;
        Gp_AnimResetSlot(&work->anim, i, work->field_9FE);
        i++;
    } while (i < 0x12);
    work->field_9FC = work->field_9FE;
}

void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_9F8 = arg2;
    work->field_9FE = arg1;
    work->field_9FA = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013DCD4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500_2", func_actor_400500_8013DD8C);

s32 func_actor_400500_8013DDEC(Task* arg0)
{
    Actor400500HitView* work = (Actor400500HitView*)arg0->idMap;

    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        return 1;
    }
    return 0;
}
