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
#include "actors/actors_shared_80139948.h"
#include "actors/actors_shared_80139c00.h"
#include "actors/actors_shared_8013a0b0.h"

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
 * `D_80115414`, from the same flag run, is declared as the aggregate
 * `actor_400600_6.c` needs, which matches here too. */
extern void* D_800678F0[1];
extern s8    D_80115414[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern u8 D_801153F4;

extern s32 D_80115738;
extern s32 D_8011574C;

extern GpU16Pair D_actor_400600_80144EA8;

extern u8 D_actor_400600_80151A48[];

void func_8017D9B8(s32);

/* One of the sub-state tables in this unit's leading rodata. The original wrote
 * it as a local array initializer, so GCC 2.8.1 put the four constant pointers
 * in the constant pool and had the dispatcher copy them onto the stack. Writing
 * that initializer here instead would emit the pool where the *function* sits in
 * the file, which is after every `INCLUDE_RODATA` above and so at the wrong
 * address; reading the splat-owned table as a `TaskFuncTable4` reproduces the
 * same copy while leaving the rodata where it is. */
extern const TaskFuncTable6 D_actor_400600_80131E54;
extern const TaskFuncTable4 D_actor_400600_80131E6C;
extern const TaskFuncTable8 D_actor_400600_80131E7C;
extern const TaskFuncTable4 D_actor_400600_80131E9C;
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

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

/* Still `INCLUDE_ASM` in this overlay; `func_actor_400600_80139CAC` is called
 * both with and without an argument, so it keeps an unprototyped declaration. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
s32  func_actor_400600_801376EC();
void func_actor_400600_80138B40(Task* arg0);
void func_actor_400600_80136558(Task* arg0);
void func_actor_400600_80136670(Task* arg0);
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
void ActorsShared8013a2c0(Task* arg0);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
s32  func_actor_400600_80137AF0(Task* arg0);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_80137498(Task* arg0, s16 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
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

void func_actor_400600_8013203C(Task* arg0)
{
    Actor400600Work* work;

    work                   = (Actor400600Work*)arg0->idMap;
    work->obj_4B4.field_8  = &((TmdObject*)arg0->extra)->field_8[3];
    work->obj_4B4.field_C  = work->rec_4D4;
    work->obj_4B4.field_10 = 0;
    work->obj_4B4.field_12 = 0x96;
    work->obj_4B4.field_14 = 0x110;
    work->obj_4B4.field_18 = 0x30006;
    if (Game_Session->field_7 == 3 && (Game_Session->field_6 == 0x1F || Game_Session->field_6 == 0x1D)) {
        work->obj_4B4.field_1C = 0x260;
    } else {
        work->obj_4B4.field_1C = 0x200;
    }
    work->obj_4B4.flags = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->rec_624.field_4  = 0xBB8;
    work->rec_624.field_10 = 0xA;
    work->rec_624.field_12 = 0xA;
    work->rec_624.field_0  = 0;
    work->rec_624.field_C  = 0;
    work->rec_624.field_8  = 0;
    work->rec_624.field_14 = work->rec_63C;
    work->obj_4B4.flags   |= 0x8000;
    work->obj_604.field_8  = ((TmdObject*)arg0->extra)->field_8;
    work->obj_604.field_C  = (GpRec18*)&work->rec_624;
    work->obj_604.field_10 = 0;
    work->obj_604.field_12 = -0x190;
    work->obj_604.field_14 = 0;
    work->obj_604.field_18 = 0x30006;
    work->obj_604.field_1C = 0;
    work->obj_604.flags    = 3;
    Gp_LinkObj(2, &work->obj_604);
    Gp_InitRec18Table(work->rec_63C, 8, 0);
    work->obj_604.flags   &= 0x3FFF;
    work->obj_594.field_18 = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_594.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj_594.field_C  = work->rec_5B4;
    work->obj_594.field_10 = -0x200;
    work->obj_594.field_12 = 0;
    work->obj_594.field_14 = 0;
    work->obj_594.field_1C = 0x190;
    work->obj_594.flags    = 1;
    Gp_LinkObj(3, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 1, 0);
    work->obj_594.flags   &= 0x7FFF;
    work->obj_5CC.field_18 = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_5CC.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj_5CC.field_C  = work->rec_5EC;
    work->obj_5CC.field_10 = 0x200;
    work->obj_5CC.field_12 = 0;
    work->obj_5CC.field_14 = 0;
    work->obj_5CC.field_1C = 0x190;
    work->obj_5CC.flags    = 1;
    Gp_LinkObj(3, &work->obj_5CC);
    Gp_InitRec18Table(work->rec_5EC, 1, 0);
    work->obj_5CC.flags &= 0x7FFF;
}

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

void func_actor_400600_801328A8(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work                = (Actor400600Work*)arg0->idMap;
    coords->coord.t[0] += (0x4364 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0x760 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= -0x508) {
        Gp_SpawnPadLerp(0xA, 0xC0, 0x80);
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x531A0009;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_8017D9B8(1);
        func_actor_400600_80139D98(arg0, 0x19, 0x30);
        coords->coord.t[1] = -0x508;
        work->field_71C++;
    }
}

void func_actor_400600_801329EC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    if ((s16)work->field_718 == 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x531A000A;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    work->field_718++;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40060004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
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

void func_actor_400600_80132B3C(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work                = (Actor400600Work*)arg0->idMap;
    coords->coord.t[0] += (0x1C54 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0xED5 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        Gp_SpawnPadLerp(0x10, 0x80, 0x40);
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x531A000A;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_actor_400600_80139D98(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->field_71C++;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132C70);

void func_actor_400600_80132E10(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    u32              sound;
    s32              pan;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC();
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x5A) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A0004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->field_71C = work->field_71C + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132F3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133118);

void func_actor_400600_801332F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    s32              mode;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    mode  = work->field_762;
    coord = model->field_8;
    if (mode == 1) {
        Gp_SpawnPadLerp(0x14, 0xFF, 0x80);
        coord->coord.t[0] = 0xCE4;
        coord->coord.t[1] = -0xBB8;
        coord->coord.t[2] = 0;
        work->field_80    = 0;
        work->field_82    = 0xC00;
        work->field_84    = 0;
        model->field_C   &= 0xFF7F;
        work->field_718   = 0;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (mode == 2) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = -0x6A4;
        coord->coord.t[2]    = -0x514;
        coord->coord.t[1]    = 0;
        work->field_82       = 0x400;
        work->field_73A      = 0xFF;
        work->field_80       = 0;
        work->field_84       = 0;
        D_80115414[0]        = mode;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E6C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131EAC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133434);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801337A8);

void func_actor_400600_80133B88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    s32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_728 < 0xBB8 || D_80115417 != 0) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060004;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_actor_400600_80138B5C(arg0, 0);
        Gp_ArmStateF0(1);
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_758  = ((rnd >> 0x10) & 0x3F) + 0x1E;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 2;
        work2->field_71E = 0;
        return;
    }
    if ((s16)func_actor_400600_80136FA8(arg0) != 0) {
        Gp_ArmStateF0(1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133CB0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133E38);

void func_actor_400600_80133FC0(Task* arg0)
{
    Actor400600Msg3FF msg;
    Actor400600Msg3F8 query;
    Actor400600Work*  work;
    Actor400600Work*  work2;
    Actor400600Work*  work3;
    s32               base;
    s32               sound;
    s32               pan;

    work = (Actor400600Work*)arg0->idMap;
    if (Gp_ActorSlots[0]->actor->field_954 == 2 || (func_actor_400600_801376EC() << 0x10) != 0 || work->field_728 >= 0x7D0 || (u32)(work->field_72C - 0x200) < 0xC01U) {
        func_actor_400600_80138B40(arg0);
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 2;
        work2->field_71E = 0;
        func_actor_400600_80135998(arg0, work->field_752);
        return;
    }
    query.field_14 = 8;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_768 == 0) {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 2;
            work3->field_71E = 0;
            return;
        }
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 0xD;
        work2->field_71E = 0;
        return;
    }
    work->field_73E = work->field_92;
    func_actor_400600_80138B40(arg0);
    work->field_768      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_767      = 1;
    work->field_9A       = work->field_92;
    msg.field_0          = D_actor_400600_80151A48;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 1;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x3FFF;
    work2                = (Actor400600Work*)arg0->idMap;
    work2->field_720     = 4;
    work2->field_726     = 0x10;
    work2->field_746     = 5;
    work2->field_742     = 1;
    work->field_718      = 0;
    work->field_71A      = 0;
    base                 = 0x40060004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_75C.b.field_75C = 0;
    work->field_71E++;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134218);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134570);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013479C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134970);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134B98);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134E28);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801350F4);

void func_actor_400600_80135450(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if ((s16)work->field_718 == 0) {
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_767 = 0;
        work->field_718++;
    }
    if ((func_actor_400600_801370F4(arg0) << 0x10) == 0 && (ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        rnd                     = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState             = rnd;
        work->field_710.h.timer = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                   = (Actor400600Work*)arg0->idMap;
        work2->field_71C        = 2;
        work2->field_71E        = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135578);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801356E0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135998);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801361AC);

void func_actor_400600_80136558(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TmdObject*       model = (TmdObject*)arg0->extra;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    s32              state = work->field_75C.word & 0xFFFF0000;
    s16              count;

    if (state == 0x1000000) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 5);
        work->field_740++;
        if (work->field_740 >= 0x20) {
            model->field_C &= ~2;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
        }
    } else if (state == 0x1010000) {
        work->field_73A = (u16)work->field_73A + (-work->field_73A >> 3);
        count           = work->field_740 + 1;
        work->field_740 = count;
        if (count >= 0x12) {
            if (work->field_75A != 0) {
                enemy->node.field_4 = 4;
            } else {
                enemy->node.field_4 = 5;
            }
            model->field_C |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
            work->field_73A             = 0;
        }
    }
}

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

/// Refreshes the parts listed in `D_actor_400600_80151B88`, projects each into
/// view space with `arg1` as the Y, and passes nine fixed pairs of the resulting
/// points to `func_actor_400600_801383E4` along with `arg2` (the fade level at
/// every call site).
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2)
{
    MATRIX         mtx;
    SVECTOR        pts[11];
    GsCOORDINATE2* coord;
    GsCOORDINATE2* root;
    s32            i;

    root              = ((TmdObject*)arg0->extra)->field_8;
    Gfx_ViewCoord.flg = 0;
    root->flg         = 0;
    for (i = 0; D_actor_400600_80151B88[i] != -1; i++) {
        coord      = &((TmdObject*)arg0->extra)->field_8[D_actor_400600_80151B88[i]];
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &mtx);
        pts[i].vx = mtx.t[0];
        pts[i].vy = arg1;
        pts[i].vz = mtx.t[2];
    }
    func_actor_400600_801383E4(&pts[1], &pts[5], 0x80, arg2);
    func_actor_400600_801383E4(&pts[5], &pts[6], 0x80, arg2);
    func_actor_400600_801383E4(&pts[1], &pts[3], 0x80, arg2);
    func_actor_400600_801383E4(&pts[3], &pts[4], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[2], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[7], 0x80, arg2);
    func_actor_400600_801383E4(&pts[7], &pts[8], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[9], 0x80, arg2);
    func_actor_400600_801383E4(&pts[9], &pts[10], 0x80, arg2);
}

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
        work->field_75C.b.field_75E = 1;
        work->field_740             = 0;
        work->field_75C.b.field_75F = 0;
        work->field_73A             = 0;
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

void func_actor_400600_80138B5C(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400600Work*)arg0->idMap;
    if (!(arg1 & 0xFF)) {
        if (work->field_75C.b.field_75E != 0) {
            work->field_75C.b.field_75E = 0;
            work->field_75C.b.field_75F = 1;
            work->field_740             = 0;
            model->field_C              = (model->field_C | 2) & 0xFF7F;
            Gp_SetLightMode(arg0->spawnArg2, 0);
            enemy->node.field_4 = 4;
            func_actor_400600_801387DC(arg0, 0);
        }
    } else if (work->field_75C.b.field_75E != 1) {
        work->field_75C.b.field_75E = 1;
        work->field_75C.b.field_75F = 1;
        work->field_740             = 0;
        model->field_C             |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
    }
}

void func_actor_400600_80138C34(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable8   fns   = D_actor_400600_80131E7C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80132704(arg0, work->field_73C, work->field_73A);
            break;
    }
}

void func_actor_400600_80138D78(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns   = D_actor_400600_80131E9C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

void func_actor_400600_80138EA0(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable6   fns   = D_actor_400600_80131E54;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, (u8)work->field_73A);
            break;
    }
}

void func_actor_400600_80138FD4(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns   = D_actor_400600_80131E6C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

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
    Actor400600Work* work;
    Actor400600Work* work2;
    SVECTOR          pos;
    s16              count;

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
        pos.vx = work->field_A8.x;
        pos.vy = work->field_A8.y;
        pos.vz = work->field_A8.z;
        ActorsShared80139c00(arg0, &pos, 0x18);
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
