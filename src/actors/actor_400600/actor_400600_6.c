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

#include "actors/actor_400600.h"
#include "actors/actors_shared_80139948.h"
#include "actors/actors_shared_80139dcc.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115417` is one byte of the run
 * of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `D_80115417` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `D_80115414`, from the same flag run, is the aggregate case too: its store
 * sits between struct stores on both sides, and the barrier trades the sink
 * for a hoist above the preceding flag updates. */
extern void* D_800678F0[1];
extern s8    D_80115414[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern u8 D_actor_400600_80151B1C[];

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);
void func_8017D9B8(s32);

/* Still `INCLUDE_ASM` in this overlay, bar the shared `ActorsShared8013a0b0`.
 * That and `func_actor_400600_80139CAC` are called both with and without an
 * argument, so they keep an unprototyped declaration and this file does not
 * include `actors_shared_8013a0b0.h`. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
void func_actor_400600_80137498(Task* arg0, s16 arg1);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_801387DC(Task* arg0, s32 arg1);
void func_actor_400600_80138AF0(Task* arg0, s32 arg1);
void func_actor_400600_80138B40(Task* arg0);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
s32  ActorsShared8013a0b0();
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013CB40(Task* arg0, u8 arg1);
void func_actor_400600_8013CB70(Task* arg0, s32 arg1);
void func_actor_400600_8013CC04(Task* arg0, s16 arg1);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013A338);

void func_actor_400600_8013A3A8(Task* arg0)
{
    ((Actor400600Work*)arg0->idMap)->field_763 = 1;
}

void func_actor_400600_8013A3B8(void)
{
}

void func_actor_400600_8013A3C0(void)
{
}

void func_actor_400600_8013A3C8(Task* arg0)
{
    GpEnemy*         enemy;
    Actor400600Work* work;
    TmdObject*       model;

    enemy                = (GpEnemy*)arg0->spawnArg2;
    work                 = (Actor400600Work*)arg0->idMap;
    model                = (TmdObject*)arg0->extra;
    work->obj_5CC.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    Gp_UnlinkNode(&enemy->node);
    if (work->field_730 == 4) {
        model->field_C |= 0x80;
        func_actor_400600_801387DC(arg0, -1);
        work->field_718 = 0;
        func_actor_400600_8013CC04(arg0, 7);
    } else if (work->field_768 == 0) {
        model->field_C &= 0xFF7F;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        func_actor_400600_801387DC(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else {
        func_actor_400600_8013CC04(arg0, 9);
    }
}

void func_actor_400600_8013A4AC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    func_actor_400600_80139D98(arg0, D_actor_400600_80151B1C[work->field_746], 0x10);
    func_actor_400600_80139CAC(arg0);
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013A518(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    func_actor_400600_80139CAC();
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A570(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;

    ((GpEnemy*)arg0->spawnArg2)->field_54 = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->obj_604);
    work->field_714 = 0x1000;
    work->matrix_0  = coord->coord;
    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);
    work->field_718 = 0;
    work->field_71C++;
}

void func_actor_400600_8013A638(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400600Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x18) {
        model->field_C |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
        work->field_718 = 0;
        work->field_71C = work->field_71C + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013A6C4);

void func_actor_400600_8013A808(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    arg0->state     = 3;
    work->field_71C = 0;
    work->field_71E = 0;
}

void func_actor_400600_8013A820(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->idMap;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 2) {
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A864(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    work  = (Actor400600Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    func_actor_400600_80137240(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->obj_604);
    work2            = (Actor400600Work*)arg0->idMap;
    arg0->state      = 3;
    work2->field_71C = 0;
    work2->field_71E = 0;
}

void func_actor_400600_8013A908(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    model           = (TmdObject*)arg0->extra;
    work            = (Actor400600Work*)arg0->idMap;
    model->field_C &= 0xFF7F;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    func_actor_400600_80139DB0(arg0, 9, 0x10, 2);
    work->field_722 = 0;
    work->field_724 = 0;
    work->field_73E = work->field_92;
    func_actor_400600_80139CAC(arg0);
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013A990(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;

    work               = (Actor400600Work*)arg0->idMap;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        func_actor_400600_80139D98(arg0, 0x13, 0x10);
        work->field_84 += 0x800;
        ActorsShared80139948(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->field_768 = 0;
        work->field_769 = 1;
        work->field_71C++;
    }
    func_actor_400600_80139CAC(arg0);
}

void func_actor_400600_8013AA5C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if ((s16)work->field_718 == 0) {
        func_actor_400600_8013CB70(arg0, 0x40060006);
        work->field_718 = work->field_718 + 1;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        func_actor_400600_8013CC04(arg0, 1);
    }
    func_actor_400600_80139CAC(arg0);
}

void func_actor_400600_8013AAD8(Task* arg0)
{
    Actor400600Work* work;
    Task*            child;
    Task*            child2;

    work       = (Actor400600Work*)arg0->idMap;
    D_80115417 = 1;
    SOFT_BARRIER();
    child = work->field_704;
    if (child != NULL) {
        Task_Kill(child);
    }
    child2 = work->field_708;
    if (child2 != NULL) {
        Task_Kill(child2);
    }
    work->field_718 = 0;
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013AB44(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->idMap;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x97) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void func_actor_400600_8013AB98(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->field_C      |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013AC14(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    coord = model->field_8;
    if (work->field_762 == 1) {
        func_8017D9B8(0);
        coord->coord.t[0] = 0x40C8;
        coord->coord.t[1] = -0x708;
        coord->coord.t[2] = -0x3E8;
        work->field_80    = 0;
        work->field_82    = 0;
        work->field_84    = 0;
        model->field_C   &= 0xFF7F;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (work->field_762 == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = 0x32C2;
        coord->coord.t[2]    = 0x960;
        coord->coord.t[1]    = 0;
        work->field_80       = 0;
        work->field_82       = 0xC00;
        work->field_84       = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_8013AD3C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        work->field_718 = 0;
        work->field_722 = 0;
        work->field_724 = 0;
        func_actor_400600_80139D98(arg0, 0x22, 0x10);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013ADA4(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    s32              x;
    s32              y;
    u16              frame;
    u16              step;
    u16              accum;

    work            = (Actor400600Work*)arg0->idMap;
    coord           = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 5) {
        x                 = coord->coord.t[0];
        coord->coord.t[0] = x + ((0x40B5 - x) >> 3);
        coord->coord.t[2] = coord->coord.t[2] + 0x78;
        step              = (u16)work->field_722 + 2;
        accum             = (u16)work->field_724 + step;
        work->field_724   = accum;
        work->field_722   = step;
        y                 = coord->coord.t[1] + (s16)accum;
        coord->coord.t[1] = y;
        if (y >= 0) {
            Gp_SpawnPadLerp(0x10, 0x80, 0x20);
            work->field_718 = 0;
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coord->coord.t[1] = 0;
            work->field_71C   = work->field_71C + 1;
        }
    }
}

void func_actor_400600_8013AE88(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->field_C      |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013AF04(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    coord = model->field_8;
    if (work->field_762 == 2) {
        coord->coord.t[0] = 0x1FDD;
        coord->coord.t[1] = -0xE38;
        coord->coord.t[2] = 0x5CE;
        work->field_80    = 0;
        work->field_82    = 0x400;
        work->field_84    = 0;
        model->field_C   &= 0xFF7F;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (work->field_762 == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = 0x640;
        coord->coord.t[2]    = 0x87A;
        coord->coord.t[1]    = 0;
        work->field_80       = 0;
        work->field_82       = 0x400;
        work->field_84       = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_8013B018(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->idMap;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

void func_actor_400600_8013B0FC(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work                 = (Actor400600Work*)arg0->idMap;
    model                = (TmdObject*)arg0->extra;
    work->field_73A      = 0;
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->field_C      |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013B150(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        Gp_ArmStateF0(1);
        work2            = (Actor400600Work*)arg0->idMap;
        arg0->state      = 1;
        work2->field_71C = 0;
        work2->field_71E = 0;
        work3            = (Actor400600Work*)arg0->idMap;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013B1DC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC();
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x50) {
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013B2A8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x404A0004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        Gp_ArmStateF0(1);
        work2            = (Actor400600Work*)arg0->idMap;
        arg0->state      = 1;
        work2->field_71C = 0;
        work2->field_71E = 0;
        work3            = (Actor400600Work*)arg0->idMap;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013B394(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->field_C      |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013B410(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              soundId;
    s32              pan;

    work                = (Actor400600Work*)arg0->idMap;
    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work->field_73A    += (0xFF - work->field_73A) >> 5;
    work->field_722    += 1;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        work->field_718 = 0;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060003;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_actor_400600_80139D98(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->field_71C++;
    }
}

void func_actor_400600_8013B520(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work            = (Actor400600Work*)arg0->idMap;
    work->field_718 = work->field_718 + 1;
    if ((s16)work->field_718 == 1) {
        Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        D_80115414[0]        = 1;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->idMap;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013B640);

void func_actor_400600_8013B6F4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_754 = 0x18;
    work->field_76B = 0;
    work->field_752 = 0x10;
    func_actor_400600_80135998(arg0, 0x10);
    work->field_71E = work->field_71E + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013B740);

void func_actor_400600_8013B830(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_769 == 0) {
        work->field_720 = 2;
        work->field_726 = 0x20;
        work->field_746 = 9;
        work->field_742 = 1;
    } else {
        work->field_720 = 2;
        work->field_726 = 0x20;
        work->field_746 = 0xB;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013B8AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_72E != 0 && work->field_730 == 1) {
        if (work->field_769 == 0) {
            work->field_726 = 0x20;
            work->field_746 = 9;
            work->field_742 = 2;
        } else {
            work->field_726 = 0x20;
            work->field_746 = 0xB;
            work->field_742 = 2;
        }
        return;
    }
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0 && (ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013B984(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_769 == 0) {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0xA;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0xC;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BA00(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013BA6C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_769 == 0) {
        work->field_720 = 8;
        work->field_726 = 0x10;
        work->field_746 = 0xF;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x11;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BAEC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_720 = 8;
            work2->field_726 = 0x10;
            work2->field_746 = 0x10;
            work2->field_742 = 1;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_720 = 8;
            work3->field_726 = 0x10;
            work3->field_746 = 0x12;
            work3->field_742 = 1;
        }
        work->field_71E = work->field_71E + 1;
    }
}

void func_actor_400600_8013BB88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013BBF4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_720 = 8;
    work->field_726 = 0x10;
    work->field_746 = 7;
    work->field_742 = 1;
    work->field_718 = 0;
    func_actor_400600_8013CB40(arg0, 0);
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BC68(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_720 = 8;
    work->field_726 = 0x10;
    work->field_746 = 8;
    work->field_742 = 1;
    work->field_718 = 0;
    func_actor_400600_8013CB40(arg0, 1);
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BCD8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->idMap;
    if ((func_actor_400600_80136FA8() << 0x10) == 0) {
        work->field_718 = 0;
        func_actor_400600_80138B5C(arg0, 0);
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_720 = 4;
        work2->field_726 = 0x10;
        work2->field_746 = 1;
        work2->field_742 = 1;
        work->field_71E  = work->field_71E + 1;
    }
}

void func_actor_400600_8013BD54(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              frame;

    work = (Actor400600Work*)arg0->idMap;
    if ((func_actor_400600_80136FA8() << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->field_718  = 0;
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_720 = 4;
            work2->field_726 = 0x10;
            work2->field_746 = 0x15;
            work2->field_742 = 1;
            work->field_71E  = work->field_71E + 1;
        }
    }
}

void func_actor_400600_8013BDF0(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work = (Actor400600Work*)arg0->idMap;
    if ((func_actor_400600_80136FA8() << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->field_71E = work->field_71E + 1;
        }
    }
}

void func_actor_400600_8013BE58(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    func_actor_400600_80137498(arg0, 0);
    work->field_763 = 0;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BE90(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    Actor400600Work* work2;

    work->field_84 += -(s16)work->field_84 >> 2;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        if (work->field_764 == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        }
        func_actor_400600_80138AF0(arg0, 0x3C);
        work->field_84   = 0;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 2;
        work2->field_71E = 0;
        work->field_767  = 0;
    }
}

void func_actor_400600_8013BF48(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    func_actor_400600_80137498(arg0, 1);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BF80(Task* arg0)
{
    Actor400600Work* work;

    if (((func_actor_400600_801370F4() << 0x10) == 0) && ((ActorsShared8013a0b0(arg0) << 0x10) != 0)) {
        work            = (Actor400600Work*)arg0->idMap;
        work->field_71C = 2;
        work->field_71E = 0;
    }
}

void func_actor_400600_8013BFD4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work             = (Actor400600Work*)arg0->idMap;
    work->field_718  = 0;
    work2            = (Actor400600Work*)arg0->idMap;
    work2->field_720 = 2;
    work2->field_726 = 0x10;
    work2->field_746 = 0x16;
    work2->field_742 = 1;
    ActorsShared80139dcc(arg0, 0xE, (ActorsShared80139dccPos*)&work->field_88);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C038(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_720 = 4;
    work->field_726 = 0x10;
    work->field_746 = 0x15;
    work->field_742 = 1;
    work->field_718 = 0;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C074(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if (((func_actor_400600_801370F4() << 0x10) == 0) && ((ActorsShared8013a0b0(arg0) << 0x10) != 0)) {
        rnd                     = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState             = rnd;
        work->field_710.h.timer = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                   = (Actor400600Work*)arg0->idMap;
        work2->field_71C        = 2;
        work2->field_71E        = 0;
    }
}

void func_actor_400600_8013C104(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_767 = 1;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C124(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    func_actor_400600_80138B40(arg0);
    work2            = (Actor400600Work*)arg0->idMap;
    work2->field_720 = 4;
    work2->field_726 = 0x10;
    work2->field_746 = 0x20;
    work2->field_742 = 1;
    work->field_722  = 0x40;
    work->field_724  = 0;
    work->field_718  = 0;
    work->field_98   = coord->coord.t[0];
    work->field_9C   = coord->coord.t[2];
    work->field_73E  = work->field_92;
    work->field_71E  = work->field_71E + 1;
}

void func_actor_400600_8013C1C0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->idMap;
    func_actor_400600_80138B5C(arg0, 0);
    work2            = (Actor400600Work*)arg0->idMap;
    work2->field_720 = 2;
    work2->field_726 = 0x10;
    work2->field_746 = 9;
    work2->field_742 = 1;
    work->field_767  = 1;
    work->field_722  = 0;
    work->field_724  = 0;
    work->field_71E  = work->field_71E + 1;
    work->field_73E  = work->field_92;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013C238);

void func_actor_400600_8013C2D4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if ((func_actor_400600_801370F4() << 0x10) != 0) {
        rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState     = rnd;
        work->field_750 = ((rnd >> 0x10) & 0x7F) + 0x1E;
    } else if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_750  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 0xA;
        work2->field_71E = 0;
    }
}

void func_actor_400600_8013C394(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_769 == 0) {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x1A;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x1B;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C410(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_720 = 0x1E;
            work2->field_726 = 0x10;
            work2->field_746 = 0x10;
            work2->field_742 = 1;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_720 = 0x1E;
            work3->field_726 = 8;
            work3->field_746 = 0x14;
            work3->field_742 = 1;
        }
        work->field_71E = work->field_71E + 1;
    }
}

void func_actor_400600_8013C4AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    if ((ActorsShared8013a0b0() << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013C518(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C534(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->idMap;
    func_actor_400600_80138B40(arg0);
    if (work->field_728 > 2000) {
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 0xD;
        work2->field_71E = 0;
    } else {
        work3            = (Actor400600Work*)arg0->idMap;
        work3->field_71C = 8;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013C598(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    u32              rnd;

    work->field_720 = 4;
    work->field_726 = 0x10;
    work->field_746 = 1;
    work->field_742 = 1;
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_756 = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C5F8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              count;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if (((func_actor_400600_80136FA8() << 0x10) == 0) && ((func_actor_400600_80137C34(arg0) << 0x10) == 0)) {
        count           = work->field_756 - 1;
        work->field_756 = count;
        if ((count << 0x10) == 0) {
            rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = rnd;
            work->field_758 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            func_actor_400600_80138B5C(arg0, 0);
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 2;
            work2->field_71E = 0;
        }
    }
}

void func_actor_400600_8013C6B0(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR v;
    VECTOR n;
    s32    dx;
    s32    dz;
    s32    dist;
    s32    t;

    dx   = pos->vx - rec->field_8;
    v.vy = 0;
    v.vx = dx;
    dz   = pos->vz - rec->field_C;
    v.vz = dz;
    dist = rec->field_2 - SquareRoot0(dx * dx + dz * dz);
    t    = dist;
    if (dist <= 0) {
        t = 0;
    }
    dist = t;
    v.vx = pos->vx - rec->field_8;
    v.vy = pos->vy - rec->field_A;
    v.vz = pos->vz - rec->field_C;
    VectorNormal(&v, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &v);
    out->vx = (dist * v.vx) >> 12;
    out->vy = 0;
    out->vz = (dist * v.vz) >> 12;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013C7E8);

void func_actor_400600_8013C874(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;

    work            = (Actor400600Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    work2           = (Actor400600Work*)arg0->idMap;
    model2          = (TmdObject*)arg0->extra;
    if (work2->field_75E != 1) {
        work2->field_75E = 1;
        work2->field_75F = 1;
        work2->field_740 = 0;
        model2->field_C |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
    }
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->field_C      |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_6", func_actor_400600_8013C940);

void func_actor_400600_8013C9DC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    work->field_718--;
    if ((s16)work->field_718 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060003;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->idMap;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

s32 func_actor_400600_8013CACC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_72E == 1 && work->field_768 == 0) {
        switch (work->field_730) {
            case 3:
                work->field_71C = 5;
                work->field_71E = 0;
                work->field_730 = 0;
                return 1;
            case 5:
                work->field_71C = 0xF;
                work->field_71E = 0;
                work->field_730 = 0;
                return 1;
        }
    }
    work->field_730 = 0;
    return 0;
}

void func_actor_400600_8013CB40(Task* arg0, u8 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    s32              mode = arg1;

    if (mode == 0) {
        work->field_766 = 1;
    } else if (mode == 1) {
        work->field_765 = mode;
    }
}

void func_actor_400600_8013CB70(Task* arg0, s32 arg1)
{
    s32 soundId;
    s32 pan;

    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        arg1 &= 0xFF00FFFF;
        arg1 |= 0x4A0000;
    }
    soundId = arg1 | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
}

void func_actor_400600_8013CC04(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_71C = arg1;
    work->field_71E = 0;
}
