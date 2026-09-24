#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_503500.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// The 0x44 block `func_actor_503500_801455A4` allocates: the shared head plus
/// the effect task it reparents itself under.
typedef struct Actor503500Work44 {
    /* 0x00 */ Actor503500ObjWork head;
    /* 0x38 */ Task*              field_38;
    /* 0x3C */ s16                field_3C; // frame counter within `field_40`'s phase
    /* 0x3E */ byte               pad_3E[0x2];
    /* 0x40 */ s8                 field_40; // phase, advanced by `func_actor_503500_80145754`
    /* 0x41 */ byte               pad_41[0x3];
} Actor503500Work44;
STATIC_ASSERT_SIZEOF(Actor503500Work44, 0x44);

/// Head of the work block used by the two enemies whose teardown clears a
/// four-entry record table (`func_actor_503500_801454E0` and
/// `func_actor_503500_80145F18`, the same body twice). It follows the gameplay
/// `GpActorD4` convention: the display node's `ctx.d4rec` points at the
/// `GpActorD4Rec` directly behind it, whose `recs` in turn points at the
/// `GpRec18` table that `Gp_InitRec18Table(_, 4, 0)` zeroes at 0x38. Like
/// `Actor503500ObjWork` this type stops where the two blocks stop agreeing:
/// `func_actor_503500_80144E8C` allocates 0xD0 and `func_actor_503500_80145A2C`
/// allocates 0xAC, both with `memCalloc(_, 0)`.
typedef struct Actor503500WorkRec4 {
    /* 0x00 */ GpObj        obj;
    /* 0x20 */ GpActorD4Rec d4;
    /* 0x38 */ GpRec18      rec[4];
} Actor503500WorkRec4;
STATIC_ASSERT_SIZEOF(Actor503500WorkRec4, 0x98);

/// The 0xD0 block `func_actor_503500_80144E8C` allocates, which is
/// `Actor503500WorkRec4` plus this task's own payload: the effect task it
/// reparents itself under, a rotation it seeds to identity next to the one in
/// its `GsCOORDINATE2`, and the pair of words plus the halfword that
/// `func_actor_503500_801450A0` reads and writes every frame.
typedef struct Actor503500WorkD0 {
    /* 0x00 */ Actor503500WorkRec4 head;
    /* 0x98 */ Task*               field_98;
    /* 0x9C */ MATRIX              field_9C;
    /* 0xBC */ GpFixed16           field_BC; // angle; the high half turns field_9C
    /* 0xC0 */ s32                 field_C0; // per-frame angle step
    /* 0xC4 */ s16                 field_C4;
    /* 0xC6 */ s16                 field_C6; // sub-state frame counter
    /* 0xC8 */ byte                pad_C8[0x4];
    /* 0xCC */ s8                  field_CC; // sub-state index
    /* 0xCD */ byte                pad_CD[0x3];
} Actor503500WorkD0;
STATIC_ASSERT_SIZEOF(Actor503500WorkD0, 0xD0);

/// The 0xAC block `func_actor_503500_80145A2C` allocates: `Actor503500WorkRec4`
/// plus the effect task it reparents itself under.
typedef struct Actor503500WorkAC {
    /* 0x00 */ Actor503500WorkRec4 head;
    /* 0x98 */ Task*               field_98;
    /* 0x9C */ byte                pad_9C[0x8];
    /* 0xA4 */ s16                 field_A4; // sub-state frame counter
    /* 0xA6 */ byte                pad_A6[0x2];
    /* 0xA8 */ s8                  field_A8; // sub-state index
    /* 0xA9 */ byte                pad_A9[0x3];
} Actor503500WorkAC;
STATIC_ASSERT_SIZEOF(Actor503500WorkAC, 0xAC);

/// The 0x4CC effect work block, allocated by `func_actor_503500_8014642C`
/// (`memCalloc(0x4CC)`) and parked in that task's `Task::work` slot -- that
/// slot is not a `TaskIdMap` here. Unlike the tasks covered by
/// `Actor503500ObjWork` this one exits through `func_actor_503500_801464E8`, which
/// only calls `Gp_EnemyTaskExit`, so the block does not open with a `GpObj`.
/// `func_actor_503500_80146508` republishes the two matrices onto
/// `TmdObject::lightMtx` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_503500_801324EC` does for `Actor503500ColorMtx`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C8 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. This is the same layout as
/// `Actor317000Work` and its siblings in the other actor overlays, except that
/// those write 0x4C8 as a halfword.
typedef struct Actor503500Effect4CC {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13]; // the slot array `func_800B3F84` is handed
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ s8         field_43C;   // set once the slots have been started
    /* 0x43D */ s8         field_43D;   // animation id the slots were seeded with
    /* 0x43E */ s8         field_43E;   // bank index into `D_actor_503500_80176520`
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ s32        field_480[4]; // saved `coord.m` words 0..3
    /* 0x490 */ s16        field_490;    // saved `coord.m[2][2]`
    /* 0x492 */ byte       pad_492[0xE];
    /* 0x4A0 */ s32        field_4A0;
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ s32        field_4B0;
    /* 0x4B4 */ s32        field_4B4;
    /* 0x4B8 */ s32        field_4B8;
    /* 0x4BC */ byte       pad_4BC[0x4];
    /* 0x4C0 */ s16        field_4C0;
    /* 0x4C2 */ s16        field_4C2;
    /* 0x4C4 */ s16        field_4C4;
    /* 0x4C6 */ s16        field_4C6;
    /* 0x4C8 */ s8         field_4C8;
    /* 0x4C9 */ byte       pad_4C9[0x3];
} Actor503500Effect4CC;
STATIC_ASSERT_SIZEOF(Actor503500Effect4CC, 0x4CC);

void func_actor_503500_801464E8(Task* arg0);
void func_actor_503500_80146508(Task* arg0);

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_503500_8014642C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80176530[];
/// Declared in `gameplay/gameplay.h`, which conflicts with this TU's headers.
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
/// Main-executable counter the actor paces periodic effects by (its value
/// modulo 6 or 12, its low bits).
extern s32     D_80070F70;
extern MATRIX* D_80073B8C;
/// Row of `Gp_PackPair` arguments, one slot per effect task
/// (`func_actor_503500_801448E8` takes the slot before this one). Declared as
/// an array because the read has to alias the struct stores around it: GCC
/// 2.8.1's `fixed_scalar_and_varying_struct_p` lets a scalar global at a fixed
/// address float above them, and hoists the load out of the call sequence.
extern GpU16Pair* D_actor_503500_8016E7D4[];
/// Local offset of the display node `func_actor_503500_80144E8C` links, and the
/// offsets it seeds its `GpActorD4Rec` with.
extern Actor503500UVec D_actor_503500_801715C4;
extern Actor503500UVec D_actor_503500_801715CC;
/// Local offset of the display node `func_actor_503500_801455A4` links.
extern Actor503500UVec D_actor_503500_801715D4;
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_80145480(Task* arg0);
void      func_actor_503500_801450A0(Actor503500* arg0);
void      func_actor_503500_801454E0(Actor503500* arg0);
void      func_actor_503500_80145754(Actor503500* arg0);
void      func_actor_503500_80145950(Task* arg0);
void      func_actor_503500_801459B0(Task* arg0);
void      func_actor_503500_80145C50(Actor503500* arg0);
void      func_actor_503500_80145F18(Actor503500* arg0);
/// libgte routine right after `RotMatrixX` in the main executable (likely
/// `RotMatrixY`); rotates `m` in place by `angle`.
void func_8004BFF8(s32 angle, MATRIX* m);
/// `func_actor_503500_80145A2C`'s `Gp_PackPair` slot (the one after
/// `D_actor_503500_8016E7D4[1]`), which the original reads by its own name, and
/// its display-node and `GpActorD4Rec` offsets.
extern GpU16Pair*      D_actor_503500_8016E7DC[];
extern Actor503500UVec D_actor_503500_801715DC;
extern Actor503500UVec D_actor_503500_801715E4;
void                   func_actor_503500_80145E98(Task* arg0);
void                   func_actor_503500_8014618C(Task* arg0);
void                   func_actor_503500_80146524(Task* arg0);
/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_503500_80176520[];
void         func_actor_503500_80144E8C(Task* arg0);
void         func_actor_503500_80145428(Actor503500* arg0);
void         func_actor_503500_801455A4(Task* arg0);
void         func_actor_503500_801458F8(Actor503500* arg0);
void         func_actor_503500_80145A2C(Task* arg0);
void         func_actor_503500_80145E1C(Actor503500* arg0);
void         func_actor_503500_8014642C(Actor503500* arg0);
void         func_actor_503500_80145FDC(Actor503500* task);
void         func_actor_503500_801464E8(Task* arg0);

/// `Task::state` handlers `func_actor_503500_8014554C` dispatches through.
const TaskFuncTable3 D_actor_503500_801321F4 = {
    {
        func_actor_503500_80144E8C,
        func_actor_503500_80145428,
        func_actor_503500_80145480,
    },
};

void func_actor_503500_80144E8C(Task* arg0)
{
    Actor503500WorkD0* work;
    GsCOORDINATE2*     coord;
    GpActorD4Rec*      d4;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m1;
    GpMtxWords*        m2;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work     = (TaskIdMap*)work;
    work->field_C4 = 0x1000;

    m1     = (GpMtxWords*)&coord->coord;
    m1->w0 = 0x1000;
    m1->w1 = 0;
    m1->w2 = 0x1000;
    m1->w3 = 0;
    m1->h4 = 0x1000;

    m2     = (GpMtxWords*)&work->field_9C;
    m2->w0 = 0x1000;
    m2->w1 = 0;
    m2->w2 = 0x1000;
    m2->w3 = 0;
    m2->h4 = 0x1000;

    d4  = &work->head.d4;
    rec = work->head.rec;

    work->head.obj.coord     = coord;
    work->head.obj.ctx.d4rec = d4;
    work->head.obj.pos.vx    = D_actor_503500_801715C4.vx;
    work->head.obj.pos.vy    = D_actor_503500_801715C4.vy;
    work->head.obj.pos.vz    = D_actor_503500_801715C4.vz;
    work->head.obj.key       = Gp_PackPair(D_actor_503500_8016E7D4[0], 0);
    work->head.obj.flags     = 3;
    work->head.obj.radius    = 0;

    d4->recs       = rec;
    d4->end1.vx    = 0;
    d4->end1.vy    = 0;
    d4->end1.vz    = 0;
    d4->end0.vx    = D_actor_503500_801715CC.vx;
    d4->end0.vy    = D_actor_503500_801715CC.vy;
    d4->end0.vz    = D_actor_503500_801715CC.vz;
    d4->end1Radius = 0x3E8;
    d4->end0Radius = 0x7D0;

    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->head.obj.flags &= 0x7FFF;

    if (arg0->spawnArg1 == 0) {
        eff = Gp_SpawnEff(0x60195, coord, 0, NULL);
        if (eff == NULL) {
            func_actor_503500_80145480(arg0);
            return;
        }
        child          = eff->task;
        work->field_98 = child;
        Task_Reparent(arg0, child);
    }
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x4023000A, pan, (s8)(gpGetObjDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145480;
    arg0->state       += 1;
}

void func_actor_503500_801450A0(Actor503500* arg0)
{
    Actor503500WorkD0* work;
    GpActorD4Rec*      d4;
    GsCOORDINATE2*     coord;
    s32                pan;
    s32                step;
    s32                ang;

    work = (Actor503500WorkD0*)arg0->field_1C;
    d4   = &work->head.d4;
    switch (work->field_CC) {
        case 0:
            if (++work->field_C6 >= 0x1F) {
                if (arg0->spawnArg1 == 0) {
                    work->head.obj.flags |= 0x8000;
                }
                coord = arg0->extra->coords;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x4023000B, pan, (s8)(gpGetObjDepth(coord) / 2));
                work->field_C6 = 0;
                work->field_CC++;
            }
            break;
        case 1:
            if (++work->field_C6 >= 0x15) {
                if (arg0->spawnArg1 != 0) {
                    work->head.obj.flags |= 0x8000;
                }
                work->field_C6 = 0;
                work->field_CC++;
            }
            break;
        case 2:
            step = -0x20000;
            if (arg0->spawnArg1 != 0) {
                step = 0x20000;
            }
            {
                GpMtxWords* m;

                m                 = (GpMtxWords*)&work->field_9C;
                m->w0             = 0x1000;
                work->field_C0   += step;
                work->field_BC.w += work->field_C0;
                m->w1             = 0;
                m->w2             = 0x1000;
                m->w3             = 0;
                m->h4             = 0x1000;
            }
            func_8004BFF8(work->field_BC.h.hi, &work->field_9C);
            ang = work->field_BC.h.hi;
            if (ang < 0) {
                ang = -ang;
            }
            if (ang > 0x100) {
                work->field_CC++;
            }
            break;
        case 3:
            if (arg0->spawnArg1 != 0) {
                work->field_C0 -= 0x20000;
                if (work->field_C0 < 0) {
                    work->field_C0 = 0;
                    work->field_CC++;
                }
            } else {
                work->field_C0 += 0x20000;
                if (work->field_C0 > 0) {
                    work->field_C0 = 0;
                    work->field_CC++;
                }
            }
            {
                GpMtxWords* m;

                m                 = (GpMtxWords*)&work->field_9C;
                m->w0             = 0x1000;
                work->field_BC.w += work->field_C0;
                m->w1             = 0;
                m->w2             = 0x1000;
                m->w3             = 0;
                m->h4             = 0x1000;
            }
            func_8004BFF8(work->field_BC.h.hi, &work->field_9C);
            break;
        case 4:
            if (work->field_C4 <= 0x400) {
                work->head.obj.flags &= 0x7FFF;
                work->field_CC++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    if (work->field_CC >= 2) {
        work->field_C4 -= 0x50;
        if (work->field_C4 < 0x200) {
            work->field_C4 = 0x200;
        }
        d4->end0Radius = work->field_C4 * 0x177 >> 9;
        d4->end1Radius = work->field_C4 * 0x7D >> 9;
        gte_SetRotMatrix(&work->field_9C);
        gte_ldv0(&D_actor_503500_801715CC);
        gte_rtv0();
        gte_stsv(d4);
    }
}

void func_actor_503500_80145428(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord      = arg0->extra->coords;
    coord->flg = 0;
    func_actor_503500_801454E0(arg0);
    func_actor_503500_801450A0(arg0);
}

void func_actor_503500_80145480(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(6);
    SndEvt_EnqueueType7(0x4023000B, 1);
    ext                                = arg0->extra;
    ((GsCOORDINATE2*)ext->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_503500_801454E0(Actor503500* arg0)
{
    Actor503500WorkRec4* work;
    GpRec18*             rec;
    s32                  i;

    work = (Actor503500WorkRec4*)arg0->field_1C;
    rec  = work->rec;
    for (i = 0; i < 4; i++) {
        if ((rec[i].key & 0xFFFF0000) == 0x10000) {
            work->obj.flags &= 0x7FFF;
        }
    }
    Gp_ClearRec18Occupied(rec);
}

void func_actor_503500_8014554C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321F4;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801459D4` dispatches through.
const TaskFuncTable3 D_actor_503500_80132218 = {
    {
        func_actor_503500_801455A4,
        func_actor_503500_801458F8,
        func_actor_503500_80145950,
    },
};

void func_actor_503500_801455A4(Task* arg0)
{
    Actor503500Work44* work;
    GsCOORDINATE2*     coord;
    GpMtxWords*        m;
    GpEffWork*         eff;
    Task*              child;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;

    m     = (GpMtxWords*)&coord->coord;
    m->w0 = 0x1000;
    m->w1 = 0;
    m->w2 = 0x1000;
    m->w3 = 0;
    m->h4 = 0x1000;

    work->head.obj.coord    = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    work->head.obj.ctx.recs = &work->head.rec;
    work->head.obj.pos.vx   = D_actor_503500_801715D4.vx;
    work->head.obj.pos.vy   = D_actor_503500_801715D4.vy;
    work->head.obj.pos.vz   = D_actor_503500_801715D4.vz;
    work->head.obj.key      = Gp_PackPair(D_actor_503500_8016E7D4[1], 0);
    work->head.obj.radius   = 0x12C;
    work->head.obj.flags    = 1;
    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(&work->head.rec, 1, 0);
    work->head.obj.flags &= 0x7FFF;

    eff = Gp_SpawnEff(0x6018A, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_80145950(arg0);
        return;
    }
    child          = eff->task;
    work->field_38 = child;
    Task_Reparent(arg0, child);
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x4023000C, pan, (s8)(gpGetObjDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145950;
    arg0->state       += 1;
}

void func_actor_503500_80145754(Actor503500* arg0)
{
    Actor503500Work44* work;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     coord2;
    s32                pan;
    s32                pan2;

    work = (Actor503500Work44*)arg0->field_1C;
    if (func_actor_503500_8013608C(arg0) == 0) {
        switch (work->field_40) {
            case 0:
                work->field_3C++;
                if (work->field_3C >= 0x5F) {
                    if (D_80073B8C->t[1] < -1000) {
                        work->head.obj.flags |= 0x8000;
                    }
                    work->field_3C = 0;
                    work->field_40++;
                } else if (work->field_3C == 0x3E) {
                    coord = arg0->extra->coords;
                    pan   = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(0x40230014, pan, (s8)(gpGetObjDepth(coord) / 2));
                } else if (work->field_3C == 0x5A) {
                    coord2 = arg0->extra->coords;
                    pan2   = (s8)Gp_GetObjPan(coord2);
                    SndEvt_EnqueueType6(0x4023000D, pan2, (s8)(gpGetObjDepth(coord2) / 2));
                }
                return;
            case 1:
                work->field_3C++;
                if (work->field_3C >= 4) {
                    work->field_3C        = 0;
                    work->head.obj.flags &= 0x7FFF;
                    work->field_40++;
                }
                return;
        }
    }
    arg0->state += 1;
}

void func_actor_503500_801458F8(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    coord = arg0->extra->coords;
    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->flg = 0;
    func_actor_503500_801459B0((Task*)arg0);
    func_actor_503500_80145754(arg0);
}

void func_actor_503500_80145950(Task* arg0)
{
    TmdObject* ext;

    SndEvt_EnqueueType7(0x4023000C, 1);
    func_actor_503500_801372AC(6);
    ext                                = arg0->extra;
    ((GsCOORDINATE2*)ext->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_503500_801459B0(Task* arg0)
{
    Gp_ClearRec18Occupied(&((Actor503500ObjWork*)arg0->work)->rec);
}

void func_actor_503500_801459D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132218;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80145F84` dispatches through.
const TaskFuncTable3 D_actor_503500_80132224 = {
    {
        func_actor_503500_80145A2C,
        func_actor_503500_80145E1C,
        func_actor_503500_80145E98,
    },
};

void func_actor_503500_80145A2C(Task* arg0)
{
    Actor503500WorkAC* work;
    GsCOORDINATE2*     coord;
    GpActorD4Rec*      d4;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    s32                pan;
    s32                pan2;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;

    m     = (GpMtxWords*)&coord->coord;
    m->w0 = 0x1000;
    m->w1 = 0;
    m->w2 = 0x1000;
    m->w3 = 0;
    m->h4 = 0x1000;

    d4  = &work->head.d4;
    rec = work->head.rec;

    work->head.obj.coord     = coord;
    work->head.obj.ctx.d4rec = d4;
    work->head.obj.pos.vx    = D_actor_503500_801715DC.vx;
    work->head.obj.pos.vy    = D_actor_503500_801715DC.vy;
    work->head.obj.pos.vz    = D_actor_503500_801715DC.vz;
    work->head.obj.key       = Gp_PackPair(D_actor_503500_8016E7DC[0], 0);
    work->head.obj.flags     = 3;
    work->head.obj.radius    = 0;

    d4->recs       = rec;
    d4->end1.vx    = 0;
    d4->end1.vy    = 0;
    d4->end1.vz    = 0;
    d4->end0.vx    = D_actor_503500_801715E4.vx;
    d4->end0.vy    = D_actor_503500_801715E4.vy;
    d4->end0.vz    = D_actor_503500_801715E4.vz;
    d4->end1Radius = 0x7D0;
    d4->end0Radius = 0xBB8;

    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->head.obj.flags &= 0x7FFF;

    eff = Gp_SpawnEff(0x60190, coord, arg0->spawnArg1, NULL);
    if (eff == NULL) {
        func_actor_503500_80145E98(arg0);
        return;
    }
    child          = eff->task;
    work->field_98 = child;
    Task_Reparent(arg0, child);
    if (gGameSession->eventState != 0) {
        pan = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0x40230013, pan, (s8)(gpGetObjDepth(coord) / 2));
    } else {
        pan2 = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0x4023000E, pan2, (s8)(gpGetObjDepth(coord) / 2));
    }
    func_actor_503500_80137290(8);
    arg0->exitCallback = func_actor_503500_80145E98;
    arg0->state       += 1;
}

void func_actor_503500_80145C50(Actor503500* arg0)
{
    Actor503500WorkAC* work;
    GsCOORDINATE2*     coord;
    s32                pan;

    work = (Actor503500WorkAC*)arg0->field_1C;
    switch (work->field_A8) {
        case 0:
            if (D_80070F70 & 1) {
                Gp_SpawnPadLerp(1, 0x96, 0x96);
            }
            if (++work->field_A4 < 0x5B) {
                return;
            }
            if (gGameSession->eventState == 0) {
                work->head.obj.flags |= 0x8000;
                coord                 = arg0->extra->coords;
                pan                   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x4023000F, pan, (s8)(gpGetObjDepth(coord) / 2));
            }
            goto next;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_SpawnPadLerp(1, 0xFF, 0xFF);
            }
            if (++work->field_A4 < 0x38) {
                return;
            }
            work->head.obj.flags &= 0x7FFF;
            SndEvt_EnqueueType7(0x4023000F, 1);
        next:
            work->field_A4 = 0;
            work->field_A8++;
            return;
        case 2:
            if (++work->field_A4 < 0x24) {
                return;
            }
        default:
            arg0->state += 1;
            return;
    }
}

void func_actor_503500_80145E1C(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    coord = arg0->extra->coords;
    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->flg = 0;
    func_actor_503500_80145F18(arg0);
    func_actor_503500_80145C50(arg0);
    if (func_actor_503500_8013608C(arg0->field_20)) {
        arg0->exitCallback((Task*)arg0);
    }
}

void func_actor_503500_80145E98(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(8);
    SndEvt_EnqueueType7(0x4023000E, 1);
    SndEvt_EnqueueType7(0x40230013, 1);
    SndEvt_EnqueueType7(0x4023000F, 1);
    ext                                = arg0->extra;
    ((GsCOORDINATE2*)ext->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_503500_80145F18(Actor503500* arg0)
{
    Actor503500WorkRec4* work;
    GpRec18*             rec;
    s32                  i;

    work = (Actor503500WorkRec4*)arg0->field_1C;
    rec  = work->rec;
    for (i = 0; i < 4; i++) {
        if ((rec[i].key & 0xFFFF0000) == 0x10000) {
            work->obj.flags &= 0x7FFF;
        }
    }
    Gp_ClearRec18Occupied(rec);
}

void func_actor_503500_80145F84(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132224;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801463C0` dispatches through.
const TaskFuncTable3 D_actor_503500_80132230 = {
    {
        func_actor_503500_8014642C,
        func_actor_503500_80145FDC,
        func_actor_503500_801464E8,
    },
};

/// Per-frame tick of the `Actor503500Effect4CC` effect: runs the motion
/// handler `field_4C0` selects, adds the 16.16 velocity `field_4B0` onto the
/// accumulator `field_4A0`, moves the coordinate by the integer part and keeps
/// only the fraction, then ticks the animation slots and the actor colour.
void func_actor_503500_80145FDC(Actor503500* task)
{
    VECTOR                pos;
    TmdObject*            ext      = task->extra;
    Actor503500Effect4CC* work     = (Actor503500Effect4CC*)task->field_1C;
    TaskFunc              funcs[2] = { func_actor_503500_80146524, func_actor_503500_8014618C };
    GsCOORDINATE2*        coord;
    s32                   i;

    funcs[work->field_4C0]((Task*)task);
    coord              = task->extra->coords;
    work->field_4A0   += work->field_4B0;
    work->field_4A4   += work->field_4B4;
    work->field_4A8   += work->field_4B8;
    coord->coord.t[0] += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1] += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2] += (s16)(work->field_4A8 >> 16);
    coord->flg         = 0;
    work->field_4A0    = (u16)work->field_4A0;
    work->field_4A4    = (u16)work->field_4A4;
    work->field_4A8    = (u16)work->field_4A8;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->field_20, &pos, 0, 0);
    }
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C8--;
    }
}

/// Motion handler 1 of `Actor503500Effect4CC` (`field_4C0`), stepped by
/// `field_4C2`: state 0 saves the coordinate's rotation words into
/// `field_480`/`field_490`, state 1 waits 31 frames, and state 2 restores that
/// rotation every frame while squashing its Y scale `field_4C6` from 0x1000 down
/// to 0x200, firing the light and spark cues on the way before advancing the
/// task at frame 150.
void func_actor_503500_8014618C(Task* arg0)
{
    VECTOR                scale;
    GsCOORDINATE2*        coord;
    Actor503500Effect4CC* work;
    TmdObject*            ext;
    void*                 enemy;
    s32*                  src;
    s32*                  dst;
    s32                   i;

    // `extra` is read twice on purpose: the second read is what leaves the
    // target's `move s2, v0` copy.
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor503500Effect4CC*)arg0->work;
    enemy = arg0->spawnArg2;
    ext   = arg0->extra;
    switch (work->field_4C2) {
        case 0:
            work->field_4C4 = 0;
            work->field_4C6 = 0x1000;
            dst             = work->field_480;
            src             = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->field_490 = coord->coord.m[2][2];
            work->field_4C2++;
            break;
        case 1:
            work->field_4C4++;
            if (work->field_4C4 >= 0x1F) {
                work->field_4C4 = 0;
                work->field_4C2++;
            }
            break;
        case 2:
            if (work->field_4C6 > 0x200) {
                work->field_4C6 -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = work->field_480;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->field_490;
            scale.vx             = 0x1000;
            scale.vy             = work->field_4C6;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            work->field_4C4++;
            switch (work->field_4C4) {
                case 0x14:
                    ext->flags |= 2;
                    Gp_SetLightMode(enemy, 1);
                    break;
                case 0x1E:
                    Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    break;
                case 0x64:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 0x96:
                    arg0->state++;
                    break;
            }
            break;
    }
}

void func_actor_503500_801463C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132230;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_503500_8014642C(Actor503500* arg0)
{
    Actor503500Effect4CC* work;
    GsCOORDINATE2*        coord;
    GpEnemy*              enemy;

    enemy = arg0->field_20;
    coord = arg0->extra->coords;

    work = memCalloc(sizeof(Actor503500Effect4CC), false);
    if (work == NULL) {
        Gp_EnemyTaskExit((Task*)arg0);
        return;
    }

    arg0->field_1C  = (Actor503500Work*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C8 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    func_actor_503500_80146508((Task*)arg0);
    TOUCH_REG(enemy);

    arg0->field_24     = D_actor_503500_80176530;
    arg0->exitCallback = func_actor_503500_801464E8;
    arg0->state++;
}
/// `Task::exitCallback` of the effect task `func_actor_503500_8014642C`
/// initialises, and the third entry of its state table: hands the task to
/// `Gp_EnemyTaskExit`.
void func_actor_503500_801464E8(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_503500_80146508(Task* arg0)
{
    TmdObject*            ext;
    Actor503500Effect4CC* work;

    work          = (Actor503500Effect4CC*)arg0->work;
    ext           = arg0->extra;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_503500_80146524(Task* arg0)
{
}

s32 func_actor_503500_8014652C(Task* task, s32 arg1, Actor503500AnimPreset* msg)
{
    Actor503500Effect4CC* work;
    TmdObject*            ext;
    s32                   i;

    work = (Actor503500Effect4CC*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_503500_80176520[work->field_43E], ext, work->field_30C,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

/// Message-0x7D4 handler of the effect task's table (the one
/// `func_actor_503500_8014642C` installs): places the effect's model at `args`
/// exactly as `func_actor_503500_80132508` places the actor. Returns 0.
s32 func_actor_503500_80146664(Task* task, s32 arg1, Actor503500PlaceArgs* args)
{
    Actor503500Coord* coord;

    coord             = (Actor503500Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_503500_801466E0(Task* task, s32 arg1, s32 mode)
{
    TmdObject* ext;
    s32        ret;

    ext = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags = (ext->flags | 0x80) & ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags                                    |= 0x80;
            ((Actor503500Effect4CC*)task->work)->field_4C8 = mode;
            ext->flags                                    |= 4;
            break;
        case 3:
            ext->flags = (ext->flags & ~0x80) | 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_801467C0(Task* task, s32 arg1, Actor503500ModeMsg* msg)
{
    Actor503500Effect4CC* work;

    work = (Actor503500Effect4CC*)task->work;
    switch (msg->mode) {
        case 0:
            work->field_4B0 = 0;
            work->field_4B4 = 0;
            work->field_4B8 = 0;
            break;
        case 1:
            work->field_4B0 = 0x0100F4DE;
            work->field_4B4 = 0xFF6DE9BE;
            work->field_4B8 = 0x68590;
            break;
        case 2:
            work->field_4B0 = 0x1371C7;
            work->field_4B4 = 0xBAAAA;
            work->field_4B8 = 0;
            work->field_4C0 = 1;
            break;
        case 3:
            task->exitCallback(task);
            break;
    }
    return 0;
}
