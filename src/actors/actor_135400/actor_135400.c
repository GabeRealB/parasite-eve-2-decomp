#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

/// Work block of the actor's second task, the one `func_actor_135400_80132B60`
/// sets up: the `memCalloc(0x498, 0)` result it stores in `Task::work`, which
/// is therefore not a `TaskIdMap` here. The main task's spawn carves a
/// different, 0x4C8-byte `Actor135400MainWork`.
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_actor_135400_80132CB0` points the `TmdObject`'s `lightMtx` /
/// `colorMtx` at them and fills them from the three `D_actor_135400_8013F904`
/// lights.
///
/// `field_43C` is the flag the 0x7D3 handler `func_actor_135400_80132D24`
/// raises once it has run the slots; `field_43D` / `field_43E` latch the
/// `field_4` / `animBlock.index` of the `GpAnimArg` that call was handed (`-1` until
/// then), and `params` holds the `D_actor_135400_80131EA0` defaults. The block
/// opens with its own animation context, the nineteen 0x28-byte slots and the
/// pose buffer the handler passes `func_800B3F84`; `func_actor_135400_801329B0`
/// ticks slots 1..18 through `Gp_AnimTickIndex` once `field_43C` is set.
typedef struct Actor135400Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       poses[0x130];
    /* 0x43C */ s8         field_43C;
    /* 0x43D */ s8         field_43D;
    /* 0x43E */ s8         field_43E;
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     lightMtx; // the model's `TmdObject::lightMtx`
    /* 0x460 */ MATRIX     colorMtx; // the model's `TmdObject::colorMtx`
    /* 0x480 */ GpAnimArg  params;
    /* 0x494 */ s32        field_494;
} Actor135400Work;
STATIC_ASSERT_SIZEOF(Actor135400Work, 0x498);

/// Work block of the actor's main task: `func_actor_135400_80132064`
/// allocates it (`memCalloc(0x4C8, 0)`) and parks the 0x7D3 / 0x7D4 / 0x7D5 /
/// 0x7DB handler table `D_actor_135400_8013A4D0` in that task's `msgTable`.
///
/// The block opens with its own animation context, the twenty 0x28-byte slots
/// and the pose buffer: `func_actor_135400_80132650`, the task's 0x7D3
/// handler, hands `func_800B3F84` the context, the pose buffer and the slots.
/// `field_474` is the live flag that handler raises once it has run the slots
/// -- `func_actor_135400_801322A8` only ticks them while it is set -- and
/// `field_475` / `field_476` are the two bytes it latches out of the animation
/// request: `field_476` indexes `D_actor_135400_8013A4A8` for the load and
/// `field_475` is passed on as the slot functions' third argument.
/// `lightMtx` / `colorMtx` are the model's own light and colour matrices,
/// which `func_actor_135400_80132634` points the `TmdObject` at.
///
/// `field_4B8` / `field_4BC` are the two part tasks the same spawn creates
/// through `Task_SpawnFromTable` (part 1 and part 2); each reparents itself
/// onto this task in its state 0 (`func_actor_135400_80132450` /
/// `func_actor_135400_8013252C`). `headAim` is the on/off latch the 0x7DB
/// handler `func_actor_135400_801328DC` sets and clears (its cases 2 and 3),
/// and `headRate` the 0x000..0xFFF rate the tick ramps toward or away from
/// the slot-3 target and hands `func_800B0928`. Only the fields decompiled
/// bodies reach are described.
typedef struct Actor135400MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       poses[0x140];
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ byte       pad_477[0x1];
    /* 0x478 */ MATRIX     lightMtx; // the model's `TmdObject::lightMtx`
    /* 0x498 */ MATRIX     colorMtx; // the model's `TmdObject::colorMtx`
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ Task*      field_4BC;
    /* 0x4C0 */ s32        headAim;
    /* 0x4C4 */ s32        headRate;
} Actor135400MainWork;
STATIC_ASSERT_SIZEOF(Actor135400MainWork, 0x4C8);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the halfword at 0x2 is the only part this actor's handler reads.
typedef struct Actor135400Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor135400Msg7DB;
STATIC_ASSERT_SIZEOF(Actor135400Msg7DB, 0x4);

/// Payload of the 0x7D4 placement message: a world translation and the Euler
/// angles handed to `RotMatrix`.
typedef struct Actor135400Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor135400Placement;
STATIC_ASSERT_SIZEOF(Actor135400Placement, 0x18);

/// `GsCOORDINATE2` at `TmdObject::coords` as the placement handlers use it:
/// the libgs `param` slot at 0x44 holds the Euler angles written there and
/// then handed straight to `RotMatrix`.
typedef struct Actor135400Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor135400Coord;
STATIC_ASSERT_SIZEOF(Actor135400Coord, 0x4C);

/// The two placements `func_actor_135400_80132064` starts the actor from. The
/// spawn copies the pair in one go and then hands the branch picked by game
/// flag 0x6C to the 0x7D4 handler `func_actor_135400_8013276C`.
typedef struct Actor135400Places {
    /* 0x00 */ Actor135400Placement field_0;
    /* 0x18 */ Actor135400Placement field_18;
} Actor135400Places;
STATIC_ASSERT_SIZEOF(Actor135400Places, 0x30);

/// A `MATRIX` plus a word-wise view of its front, which
/// `func_actor_135400_80131EB4` uses to write an identity rotation in five
/// aligned stores before `RotMatrixY` overwrites it.
typedef union Actor135400Mat {
    MATRIX mat;
    struct {
        s32 m00_m01;
        s32 m02_m10;
        s32 m11_m12;
        s32 m20_m21;
        s16 m22;
    } ident;
} Actor135400Mat;
STATIC_ASSERT_SIZEOF(Actor135400Mat, 0x20);

/// The actor's two-entry `TaskDesc` table, indexed by `Task_SpawnFromTable`:
/// entry 1 is the model-bearing part task `func_actor_135400_80132450`
/// reparents, entry 2 the second part (`func_actor_135400_8013252C`).
extern TaskDesc D_actor_135400_8013A4AC;

/// The handler table `func_actor_135400_80132064` parks in `Task::msgTable`:
/// the 0x7D3 / 0x7D4 / 0x7D5 / 0x7DB handlers of the main task.
extern s32 D_actor_135400_8013A4D0;

/// The three flat lights `func_actor_135400_80132CB0` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

/// The second task's message table: `(message id, handler)` pairs for 0x7D3 /
/// 0x7D4 / 0x7D5, ended by `0x7FFFFFFF`. `func_actor_135400_80132B60` parks
/// its address in `Task::msgTable`.
extern s32 D_actor_135400_8013F8E4;

/// Per-step frame counts of the second task's 0x7D3 animation: eight `s16`
/// entries indexed by `Actor135400Work::params.field_4`.
/// `func_actor_135400_801329B0` runs the task's `killCountdown` up and, once it
/// passes the entry for the current step, advances that step -- wrapping at 7
/// -- and re-issues the animation. The first and last entries are zero, so
/// neither ever expires.
extern s16 D_actor_135400_8013F8C4[];

/// Animation banks the two 0x7D3 handlers re-seed their slots from, indexed by
/// the request's `field_0`: `D_actor_135400_8013A4A8` for the main task,
/// `D_actor_135400_8013F8D4` for the second task.
extern void* D_actor_135400_8013A4A8[];
extern void* D_actor_135400_8013F8D4[];

/// Psy-Q `RotMatrixY`: the angle is a `long`, so a negated angle is passed
/// without re-truncation to 16 bits.
void func_8004BFF8(s32 angle, MATRIX* matrix);

/// Main-executable helper the spawn runs on the flag-clear path, once the
/// actor is placed. Unmatched, so declared here.
void func_80180414(s32 arg0);

/// Declared here with a signed `arg2`: every caller passes a sign-extended
/// animation id, which the definition's `u16` prototype would re-extend.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_135400_80131EB4(Task* task);
void func_actor_135400_80132064(Task* arg0);
void func_actor_135400_801322A8(Task* task);
void func_actor_135400_80132450(Task* task);
void func_actor_135400_801324CC(Task* task);
void func_actor_135400_8013252C(Task* task);
void func_actor_135400_80132614(Task* arg0);
void func_actor_135400_80132634(Task* task);
s32  func_actor_135400_80132650(Task* task, s32 anim, GpAnimArg* params, s32 arg3);
s32  func_actor_135400_8013276C(Task* task, s32 anim, Actor135400Placement* args, s32 arg3);
s32  func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3);
void func_actor_135400_80132B60(Task* arg0);
void func_actor_135400_80132C90(Task* arg0);
void func_actor_135400_80132CB0(Task* task);
s32  func_actor_135400_80132D24(Task* task, s32 anim, GpAnimArg* params, s32 arg3);
s32  func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3);

/// State table of the first part task: state 0 reparents it
/// (`func_actor_135400_80132450`), state 1 does nothing and state 2 kills it.
/// Dispatched by `func_actor_135400_801323F8`.
const TaskFuncTable3 D_actor_135400_80131E24 = { {
    func_actor_135400_80132450,
    func_actor_135400_801324CC,
    taskKill,
} };

/// State table of the second part task: state 0 reparents it
/// (`func_actor_135400_8013252C`), state 1 runs its placement phases
/// (`func_actor_135400_80131EB4`) and state 2 kills it. Dispatched by
/// `func_actor_135400_801324D4`.
const TaskFuncTable3 D_actor_135400_80131E30 = { {
    func_actor_135400_8013252C,
    func_actor_135400_80131EB4,
    taskKill,
} };

/// State table of the main task: spawn, per-frame tick and exit callback.
/// Dispatched by `func_actor_135400_801325A8`.
const TaskFuncTable3 D_actor_135400_80131E3C = { {
    func_actor_135400_80132064,
    func_actor_135400_801322A8,
    func_actor_135400_80132614,
} };

/// The two spawn placements `func_actor_135400_80132064` copies as a whole:
/// the flag-clear branch's first, the other second.
const Actor135400Places D_actor_135400_80131E48 = {
    { { 5700, -150, 5900, 0 }, { 1024, 0, -1024, 0 } },
    { { 4700, 0, 5000, 0 }, { 0, -1024, 0, 0 } },
};

/// Second state handler of the actor's part-2 table (`D_actor_135400_80131E30`,
/// dispatched by `func_actor_135400_801324D4`): a three-phase machine run off
/// `Task::spawnArg1`, the slot the part's state-0 handler read as the part
/// index and sets to 2 for the second part. Phase 1 bakes the part's
/// parent-relative coordinate into world space with `Gp_ComposeParentWorld` and
/// reparents it to `gGfxViewCoord`. Phases 2 and 3 share a body -- 2 only
/// reaches it while the session's `eventState` is clear -- which resets the
/// coordinate to a `-0x38E` yaw (`func_8004BFF8`, `RotMatrixY`) at the fixed
/// world position (0x12FE, -0x1B3, 0x157C) and drops the phase back to 0.
void func_actor_135400_80131EB4(Task* task)
{
    Actor135400Mat  rot;
    Actor135400Mat* src;
    SVECTOR         sv;
    GsCOORDINATE2*  coord;

    switch (task->spawnArg1) {
        case 1:
            coord = ((TmdObject*)task->extra)->coords;
            Gp_ComposeParentWorld(coord, &rot.mat, &sv);
            coord->coord      = rot.mat;
            coord->coord.t[0] = sv.vx;
            coord->coord.t[1] = sv.vy;
            coord->coord.t[2] = sv.vz;
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            task->spawnArg1  += 1;
            break;
        case 2:
            if (gGameSession->eventState != 0) {
                break;
            }
        case 3:
            coord              = ((TmdObject*)task->extra)->coords;
            src                = &rot;
            src->ident.m00_m01 = 0x1000;
            src->ident.m02_m10 = 0;
            src->ident.m11_m12 = 0x1000;
            src->ident.m20_m21 = 0;
            src->ident.m22     = 0x1000;
            func_8004BFF8(-0x38E, &rot.mat);
            coord->coord      = rot.mat;
            coord->coord.t[0] = 0x12FE;
            coord->coord.t[1] = -0x1B3;
            coord->coord.t[2] = 0x157C;
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            task->spawnArg1   = 0;
            break;
    }
}

/// The spawn handler of the actor's main task: carves the 0x4C8-byte work
/// block, seeds its two `-1` latches, starts the two part tasks and copies the
/// area record's texture page / CLUT onto part 1's model. It then installs the
/// handler table, the 0x7D5 model mode and the exit callback, and finally hands
/// the 0x7D4 placement and the 0x7D3 animation the game flag 0x6C selects.
///
/// `keyPtr` / `TOUCH_REG` are load-bearing: the `&key` argument comes out of
/// `expand_call` in a fresh pseudo, and left alone `cse` folds the second call
/// site into it, so the address lives across the first call and costs a
/// callee-saved register. The touch makes the second call materialise it afresh
/// -- the ROM's two `addiu $a0,$sp,0x68`.
void func_actor_135400_80132064(Task* arg0)
{
    Actor135400MainWork* work;
    Actor135400Places    places;
    GpAnimArg            anim[2];
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    GpAreaKey*           keyPtr;
    u8                   areaByte0;
    GpAreaRec*           rec;
    GpAreaPlace*         place;
    TmdObject*           model;
    Task*                spawned;
    u32                  raw;
    s32                  idx;

    places = D_actor_135400_80131E48;
    memset(anim, 0, sizeof(anim));
    anim[0].field_4 = 1;
    anim[1].field_4 = 4;
    work            = (Actor135400MainWork*)memCalloc(0x4C8, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    spawned         = Task_SpawnFromTable(&D_actor_135400_8013A4AC, 1, 4, (s32)arg0);
    if (spawned != NULL) {
        work->field_4B8 = spawned;
        model           = (TmdObject*)spawned->extra;
        sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
        raw             = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        areaByte0       = sessionKey->view;
        keyPtr          = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        idx      = raw >> 12;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_135400_8013A4AC, 2, 8, (s32)arg0);
    if (spawned != NULL) {
        work->field_4BC = spawned;
    }
    func_actor_135400_80132634(arg0);
    arg0->msgTable = &D_actor_135400_8013A4D0;
    func_actor_135400_801327E8(arg0, 0x7D5, 1, 0);
    if (GameFlag_GetNibble(0x6C) <= 0) {
        func_actor_135400_8013276C(arg0, 0x7D4, &places.field_0, 0);
        func_actor_135400_80132650(arg0, 0x7D3, &anim[0], 0);
        func_80180414(0);
    } else {
        func_actor_135400_8013276C(arg0, 0x7D4, &places.field_18, 0);
        func_actor_135400_80132650(arg0, 0x7D3, &anim[1], 0);
    }
    arg0->exitCallback = func_actor_135400_80132614;
    arg0->state       += 1;
}

/// Per-frame tick of the actor's main task: ticks the twenty animation slots
/// once `field_474` has latched, and while the model is not hidden (flag 0x80
/// of `TmdObject::flags`) draws its ground shadow from the second
/// part's translation, recomputes that part's world matrix, re-ranks it
/// through `func_800D7A9C`, ramps the head-tracking rate `headRate` and finally
/// turns the head toward the slot-3 skeleton with `func_800B0928`.
void func_actor_135400_801322A8(Task* task)
{
    Actor135400MainWork* work;
    TmdObject*           ext;
    VECTOR3              pos;
    s32                  i;
    s32                  rate;

    work = (Actor135400MainWork*)task->work;
    ext  = task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        if (work->headAim != 0) {
            rate           = work->headRate + 0x100;
            work->headRate = rate;
            if (rate >= 0x1000) {
                work->headRate = 0xFFF;
            }
        } else {
            rate           = work->headRate - 0x80;
            work->headRate = rate;
            if (rate < 0) {
                work->headRate = 0;
            }
        }
        func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, work->headRate);
    }
}

/// Per-frame dispatcher of the first part task: runs its state from
/// `D_actor_135400_80131E24`.
void func_actor_135400_801323F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E24;
    sp.funcs[task->state](task);
}

/// State 0 of the first part task (`D_actor_135400_80131E24`): hangs the
/// part's root coordinate under the parent's coordinate that `spawnArg1`
/// indexes, shares the parent model's light and colour matrices and reparents
/// the task onto the parent before advancing the state.
void func_actor_135400_80132450(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// State 1 of the first part task: nothing to do while it rides its parent.
void func_actor_135400_801324CC(Task* task)
{
}

/// Per-frame dispatcher of the second part task: runs its state from
/// `D_actor_135400_80131E30`.
void func_actor_135400_801324D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E30;
    sp.funcs[task->state](task);
}

/// State 0 of the second part task (`D_actor_135400_80131E30`): hangs the
/// part's root coordinate under the parent's coordinate that `spawnArg1`
/// indexes, shares the parent model's light and colour matrices and reparents
/// the task onto the parent before advancing the state.
void func_actor_135400_8013252C(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_135400_80131E3C`, skipping the frame while `Gp_StateF0.field_4` is
/// set.
void func_actor_135400_801325A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E3C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// The main task's exit callback, also its state 2: runs the enemy teardown.
void func_actor_135400_80132614(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the main task's model at the work block's own light and colour
/// matrices, so it draws with the actor's lighting rather than the defaults.
/// The spawn handler runs it once the two part tasks are started.
void func_actor_135400_80132634(Task* task)
{
    TmdObject*           ext;
    Actor135400MainWork* work;

    ext           = task->extra;
    work          = (Actor135400MainWork*)task->work;
    ext->lightMtx = &work->lightMtx;
    ext->colorMtx = &work->colorMtx;
}

/// The main task's 0x7D3 handler: when the request names a different bank
/// than the one latched in `field_476`, re-seeds the twenty slots from
/// `D_actor_135400_8013A4A8`; then, with `field_8` set and the slots already
/// live, starts animation `field_4` on every slot through `func_800B4114`
/// (passing `field_C`), otherwise resets every slot to it, and ticks them all
/// once.
s32 func_actor_135400_80132650(Task* task, s32 anim, GpAnimArg* params, s32 arg3)
{
    Actor135400MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor135400MainWork*)task->work;
    ext  = task->extra;
    if (params->animBlock.index != work->field_476) {
        work->field_476 = params->animBlock.index;
        func_800B3F84(&work->anim, D_actor_135400_8013A4A8[work->field_476], ext, work->poses, work->slots);
    }
    work->field_475 = params->field_4;
    if (params->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, params->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}

/// The main task's 0x7D4 handler: drops the placement's translation into the
/// root part's local matrix and its Euler angles into the coordinate's `rot`
/// slot, rebuilds the rotation from them and clears `flg` so the world matrix
/// is recomputed.
s32 func_actor_135400_8013276C(Task* task, s32 anim, Actor135400Placement* args, s32 arg3)
{
    Actor135400Coord* coord;

    coord             = (Actor135400Coord*)((TmdObject*)task->extra)->coords;
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

/// The main task's 0x7D5 handler, a four-way model mode switch on the
/// `TmdObject` in `Task::extra`. Mode 0 hides the model (flag 0x80) and clears
/// flag 0x4; 1 shows it, allocates its buffers and clears 0x4; 2 hides it,
/// frees the buffers and sets 0x4; 3 shows it and sets 0x4. Anything else
/// returns 1 and leaves the model alone; the handled modes return 0. Either
/// way the resulting flags are copied onto the first part task's model
/// (`Actor135400MainWork::field_4B8`), keeping the pair in step.
s32 func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    TmdObject* obj;
    TmdObject* other;
    s32        ret;

    obj   = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor135400MainWork*)task->work)->field_4B8->extra;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags |= 0x80;
            Tmd_FreeBuffers(obj);
            obj->flags |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}

/// The main task's 0x7DB handler. The payload halfword picks one of six
/// actions against the second part task (`Actor135400MainWork::field_4BC`):
/// 0 and 1 show and hide that task's model (flag 0x80), 2 and 3 set
/// and clear `headAim`, 4 hands the part task a `spawnArg1` of 1, and 5 sets
/// that to 3 and then shows the model. Nothing reads the message id.
s32 func_actor_135400_801328DC(Task* task, s32 msgId, Actor135400Msg7DB* msg, s32 arg3)
{
    Actor135400MainWork* work;
    TmdObject*           model;

    work = (Actor135400MainWork*)task->work;
    switch (msg->field_2) {
        case 0:
            if (work->field_4BC != NULL) {
                model         = (TmdObject*)work->field_4BC->extra;
                model->flags &= 0xFF7F;
            }
            break;
        case 1:
            if (work->field_4BC != NULL) {
                model         = (TmdObject*)work->field_4BC->extra;
                model->flags |= 0x80;
            }
            break;
        case 2:
            work->headAim = 1;
            break;
        case 3:
            work->headAim = 0;
            break;
        case 4:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1 = 1;
            }
            break;
        case 5:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1 = 3;
                model                      = (TmdObject*)work->field_4BC->extra;
                model->flags              &= 0xFF7F;
            }
            break;
    }
    return 0;
}

/// Per-frame tick of the second task. Once `func_actor_135400_80132D24` has
/// raised `field_43C` it ticks slots 1..18 of the work block through
/// `Gp_AnimTickIndex`; while the model is not hidden (flag 0x80 of
/// `TmdObject::flags`) it draws the ground shadow under the model's
/// root part, as `func_actor_135400_801322A8` does for the main task. It then
/// steps the 0x7D3 animation on the `D_actor_135400_8013F8C4` frame counts, and
/// finally runs `field_494` down -- at zero the model's aux buffers are freed
/// and the countdown carries on to -1, so that free happens once.
void func_actor_135400_801329B0(Task* task)
{
    Actor135400Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s32              i;
    s32              step;
    u16              count;

    work = (Actor135400Work*)task->work;
    ext  = task->extra;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80) && (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[0].workm.t, &pos) != 0)) {
        Gp_DrawEffGroundQuad(&pos, 0x180, Gp_State1C->groundShade);
    }
    count               = task->killCountdown + 1;
    task->killCountdown = count;
    if (D_actor_135400_8013F8C4[work->params.field_4] < (s16)count) {
        work->params.field_4 = work->params.field_4 + 1;
        if (work->params.field_4 >= 7) {
            work->params.field_4 = 1;
        }
        func_actor_135400_80132D24(task, 0x7D3, &work->params, 0);
        task->killCountdown = 0;
    }
    step = work->field_494;
    if (step >= 0) {
        if (step == 0) {
            Tmd_FreeBuffers(ext);
            step = work->field_494;
        }
        step           -= 1;
        work->field_494 = step;
    }
}

/// State table of the second task: spawn, per-frame tick and exit callback.
/// Dispatched by `func_actor_135400_80132AF4`.
const TaskFuncTable3 D_actor_135400_80131E94 = { {
    func_actor_135400_80132B60,
    func_actor_135400_801329B0,
    func_actor_135400_80132C90,
} };

/// Per-frame dispatcher of the task `func_actor_135400_80132B60` sets up: runs
/// its spawn, tick or exit state from `D_actor_135400_80131E94`, skipping the
/// frame while `Gp_StateF0.field_4` is set.
void func_actor_135400_80132AF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E94;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// The animation arguments `func_actor_135400_80132B60` copies into
/// `Actor135400Work::params` when the second task is created.
const GpAnimArg D_actor_135400_80131EA0 = { 0, 2, 1, 10, 0 };

/// Spawn state of the second task: exits at once when game flag 0x6C is set or
/// the 0x498-byte work block cannot be allocated. Otherwise it seeds the
/// block's latches, stores the `D_actor_135400_80131EA0` defaults, starts
/// animation 1, shows the model through the 0x7D5 handler, loads its flat
/// lights, and installs the message table and exit callback.
void func_actor_135400_80132B60(Task* arg0)
{
    Actor135400Work* work;
    GpAnimArg        params;
    GpAnimArg        spawn;

    memset(&params, 0, sizeof(params));
    params.field_4 = 1;
    spawn          = D_actor_135400_80131EA0;
    if ((GameFlag_GetNibble(0x6C) > 0) || ((work = memCalloc(0x498, 0)) == NULL)) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_494 = -1;
    work->params    = spawn;
    func_actor_135400_80132D24(arg0, 0x7D3, &params, 0);
    func_actor_135400_80132EBC(arg0, 0x7D5, 1, 0);
    func_actor_135400_80132CB0(arg0);
    arg0->msgTable     = &D_actor_135400_8013F8E4;
    arg0->exitCallback = func_actor_135400_80132C90;
    arg0->state       += 1;
}

/// The second task's exit callback, also its state 2: runs the enemy teardown.
void func_actor_135400_80132C90(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the second task's model at the work block's light and colour
/// matrices and fills them from the three `D_actor_135400_8013F904` lights.
void func_actor_135400_80132CB0(Task* task)
{
    Actor135400Work* work = (Actor135400Work*)task->work;
    TmdObject*       obj  = (TmdObject*)task->extra;
    GsF_LIGHT*       light;
    s32              i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_actor_135400_8013F904; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// The 0x7D3 handler of the task `func_actor_135400_80132B60` sets up: when
/// the request names a different bank than the one latched in `field_43E`,
/// re-seeds the nineteen slots from `D_actor_135400_8013F8D4`; then, with
/// `field_8` set and the slots already live, starts animation `field_4` on
/// every slot through `func_800B4114` (passing `field_C`), otherwise resets
/// every slot to it, and ticks them all once.
s32 func_actor_135400_80132D24(Task* task, s32 anim, GpAnimArg* params, s32 arg3)
{
    Actor135400Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor135400Work*)task->work;
    ext  = task->extra;
    if (params->animBlock.index != work->field_43E) {
        work->field_43E = params->animBlock.index;
        func_800B3F84(&work->anim, D_actor_135400_8013F8D4[work->field_43E], ext, work->poses, work->slots);
    }
    work->field_43D = params->field_4;
    if (params->field_8 != 0 && work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, work->field_43D, 0, params->field_C);
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
    return 0;
}

/// The 0x7D4 handler of the same task: the same placement as
/// `func_actor_135400_8013276C`, applied to this task's root coordinate.
s32 func_actor_135400_80132E40(Task* task, s32 anim, Actor135400Placement* args, s32 arg3)
{
    Actor135400Coord* coord;

    coord             = (Actor135400Coord*)((TmdObject*)task->extra)->coords;
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

/// The second task's 0x7D5 handler, the same mode switch as
/// `func_actor_135400_801327E8` on this task's model alone. Mode 2 does not
/// free the buffers itself: it stores 2 in `field_494`, and the tick frees
/// them once that countdown reaches zero.
s32 func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (arg2) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor135400Work*)task->work)->field_494 = arg2;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}
