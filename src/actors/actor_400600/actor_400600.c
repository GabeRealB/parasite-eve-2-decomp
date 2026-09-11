#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"

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
 * aggregate, so its one-element array stays and is doing real work. */
extern void* D_800678F0[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern s32 D_80115738;
extern s32 D_8011574C;

/* One of the sub-state tables in this unit's leading rodata. The original wrote
 * it as a local array initializer, so GCC 2.8.1 put the four constant pointers
 * in the constant pool and had the dispatcher copy them onto the stack. Writing
 * that initializer here instead would emit the pool where the *function* sits in
 * the file, which is after every `INCLUDE_RODATA` above and so at the wrong
 * address; reading the splat-owned table as a `TaskFuncTable4` reproduces the
 * same copy while leaving the rodata where it is. */
extern const TaskFuncTable3 D_actor_400600_80131F34;
extern const TaskFuncTable8 D_actor_400600_80131F40;
extern const TaskFuncTable4 D_actor_400600_80131F60;
extern const TaskFuncTable3 D_actor_400600_80131F70;
extern const TaskFuncTable4 D_actor_400600_80131F7C;
extern const TaskFuncTable4 D_actor_400600_80131F8C;
extern const TaskFuncTable3 D_actor_400600_80131F9C;

extern u8 D_actor_400600_8014220C[];
extern u8 D_actor_400600_80143604[];
extern u8 D_actor_400600_80143B24[];
extern u8 D_actor_400600_80144994[];

/* Still `INCLUDE_ASM` in this overlay; `func_actor_400600_80139CAC` and
 * `func_actor_400600_8013A0B0` are called both with and without an argument,
 * so they keep an unprototyped declaration. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
s32  func_actor_400600_80137AF0(Task* arg0);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_80137498(Task* arg0, s16 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
s32  func_actor_400600_8013A0B0();
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_8013B6F4(Task* arg0);
void func_actor_400600_8013B740(Task* arg0);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013B984(Task* arg0);
void func_actor_400600_8013BA00(Task* arg0);
void func_actor_400600_8013BC68(Task* arg0);
void func_actor_400600_80133E38(Task* arg0);
void func_actor_400600_8013BBF4(Task* arg0);
void func_actor_400600_80133CB0(Task* arg0);
void func_actor_400600_8013BFD4(Task* arg0);
void func_actor_400600_80134B98(Task* arg0);
void func_actor_400600_8013C518(Task* arg0);
void func_actor_400600_8013C534(Task* arg0);
void func_actor_400600_8013C598(Task* arg0);
s32  func_actor_400600_8013CACC(Task* arg0);
void func_actor_400600_8013C5F8(Task* arg0);
void func_actor_400600_80132294(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);
void func_actor_400600_80135DDC(Task* arg0);
void func_actor_400600_80139C00(Task* arg0, Actor400600ViewPos* arg1, s32 arg2);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013203C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132294);

void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_400600_80132294(arg0, 3, 9, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 9, 0xA, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xA, 0xB, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 3, 6, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 6, 7, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 7, 8, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 5, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xC, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xC, 0xD, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xD, 0xE, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xF, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xF, 0x10, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0x10, 0x11, 0x80, arg1, arg2);
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801328A8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801329EC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132B3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132C70);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132E10);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132F3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133118);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801332F4);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E6C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131EAC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133434);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801337A8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133B88);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133CB0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133E38);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134218);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134570);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013479C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134970);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134B98);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134E28);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801350F4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135450);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135578);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801356E0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135998);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801361AC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136558);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136670);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F34);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F40);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F60);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F70);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F8C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136968);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136FA8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801370F4);

void func_actor_400600_80137240(Task* arg0)
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

    D_800678F0[0] = D_actor_400600_8014220C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[4], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80143604;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80143B24;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[16], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80144994;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4           = (TmdObject*)arg0->extra;
        dst4           = (TmdObject*)eff4->field_0->extra;
        dst4->field_24 = src4->field_24;
        dst4->field_25 = src4->field_25;
        if (dst4->field_18 != NULL) {
            Tmd_ProcessStream(dst4);
            Tmd_ProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801376EC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137840);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137AF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137C34);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137EF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138224);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801383E4);

/// Copies this actor's model flags onto both child tasks' models and, for a
/// non-negative `arg1`, sets the children's light mode to it.
void func_actor_400600_801387DC(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Task*            child;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    if (work->field_704 != NULL) {
        child                               = work->field_704;
        ((TmdObject*)child->extra)->field_C = model->field_C;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
    if (work->field_708 != NULL) {
        child                               = work->field_708;
        ((TmdObject*)child->extra)->field_C = model->field_C;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013886C);

s32 func_actor_400600_8013892C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if ((work->field_710.h.flags & 1) || (work->field_710.word & 0x01020000)) {
        return 1;
    }
    return 0;
}

void func_actor_400600_8013896C(Task* arg0, s16 arg1)
{
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->field_8;
    Gp_SpawnEff(D_8011574C, coord, 0x40, NULL);
    for (i = 0; i < 16; i++) {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = arg1;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
    }
}

void func_actor_400600_80138A24(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if (arg1 != 0) {
        enemy->node.field_4 = 5;
        model->field_C     |= 0x80;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
        work->field_75E = 1;
        work->field_740 = 0;
        work->field_75F = 0;
        work->field_73A = 0;
    }
}

void func_actor_400600_80138AA4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_766 = 0;
    work->field_765 = 0;
}

void func_actor_400600_80138AB8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_732 > 0) {
        work->field_732 = (u16)work->field_732 - 1;
    }
    if (work->field_710.h.timer > 0) {
        work->field_710.h.timer = (u16)work->field_710.h.timer - 1;
    }
}

void func_actor_400600_80138AF0(Task* arg0, s32 arg1)
{
    u32 rnd1;
    u32 rnd2;

    rnd1                                       = ((u32)Gp_LcgState * 5) + 0x71357911;
    rnd2                                       = (rnd1 * 5) + 0x71357911;
    Gp_LcgState                                = rnd2;
    ((Actor400600Work*)arg0->idMap)->field_732 = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
}

void func_actor_400600_80138B40(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->obj_604.flags &= 0xBFFF;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138C34);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138D78);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138EA0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138FD4);

void func_actor_400600_801390FC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_71C = 1;
    work->field_71E = 0;
}

void func_actor_400600_80139110(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
        if ((s16)func_actor_400600_80137C34(arg0) == 0 && (s16)func_actor_400600_80137AF0(arg0) == 0 && (*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x4080000 && work->field_768 != 0 && ((TmdObject*)arg0->extra)->field_8->coord.t[0] > 10000) {
            Actor400600Work* cur = (Actor400600Work*)arg0->idMap;

            cur->field_71C = 0xD;
            cur->field_71E = 0;
        }
    }
}

void func_actor_400600_80139218(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B830, func_actor_400600_8013B8AC };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139280(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B984, func_actor_400600_8013BA00 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801392E8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F34;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_8013935C(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BBF4, func_actor_400600_80133CB0 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_801393D0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BC68, func_actor_400600_80133E38 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139444(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable8   fns  = D_actor_400600_80131F40;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801394E0(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F60;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139560(Task* arg0)
{
    Actor400600Work*   work;
    Actor400600Work*   work2;
    Actor400600ViewPos pos;
    s16                count;

    work = (Actor400600Work*)arg0->idMap;
    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        count           = work->field_750 - 1;
        work->field_750 = count;
        if (count == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 0xB;
            work2->field_71E = 0;
            return;
        }
        pos.x = work->field_A8.x;
        pos.y = work->field_A8.y;
        pos.z = work->field_A8.z;
        func_actor_400600_80139C00(arg0, &pos, 0x18);
        func_actor_400600_80135DDC(arg0);
    }
}

void func_actor_400600_80139608(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BFD4, func_actor_400600_80134B98 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139670(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F70;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801396E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F7C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139764(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F8C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801397E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F9C;

    func_actor_400600_80138AA4(arg0);
    if ((func_actor_400600_8013CACC(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139878(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C518, func_actor_400600_8013C534 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801398E0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C598, func_actor_400600_8013C5F8 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139948);

void func_actor_400600_80139A78(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = work->field_726;
        Gp_AnimResetSlot(&work->anim, i, work->field_746);
        i++;
    } while (i < 0x12);
    work->field_744 = work->field_746;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139AE8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139BA0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139C00);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139CAC);

void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 2;
}

void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_720 = arg3;
    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 1;
}
