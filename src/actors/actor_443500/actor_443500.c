#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// Position context the spawn handler reads through `Task::spawnArg2` (0x20):
/// the halfword at 0x8 packs the area position, shifted down by 12 into the
/// index `Gp_GetNestedAreaRec`'s table is walked with.
typedef struct Actor443500Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor443500Ctx;

/// Work block `func_actor_443500_80132078` `memCalloc`s (0x4C4) and parks in
/// the task's `Task::work` slot, which holds no `TaskIdMap` here. The spawn
/// handler seeds the two `sb` bytes at 0x475/0x476 and the word at 0x4BC to
/// -1 and copies the parent TmdObject's flags halfword to 0x4C0; the
/// light/colour matrix pair at 0x478/0x498 is the one
/// `func_actor_443500_801327C4` republishes onto the model.
///
/// The size is the allocation; the fields below are the ones this overlay's
/// decompiled bodies touch.
typedef struct Actor443500Work {
    /* 0x000 */ GpAnimCtx anim;
    /// The twenty slots `func_actor_443500_801327E0` walks 1..0x13, each
    /// 0x28 bytes, filling the span up to the matrix table.
    /* 0x014 */ GpAnimSlot slots[0x14];
    /// The matrix table `func_800B3F84` fills, immediately after the slots.
    /* 0x334 */ byte field_334[0x140];
    /// Raised by `func_actor_443500_801327E0` after its last slot pass, and
    /// the condition its start branch reads: the first pass, while it is still
    /// clear, clears the slots instead.
    /* 0x474 */ s8     field_474;
    /* 0x475 */ s8     field_475;
    /* 0x476 */ s8     field_476;
    /* 0x477 */ byte   pad_477[0x1];
    /* 0x478 */ MATRIX light;
    /* 0x498 */ MATRIX color;
    /* 0x4B8 */ byte   pad_4B8[0x2];
    /// Cleared by `func_actor_443500_801327E0` after the slot passes, beside
    /// the `field_474` latch it raises.
    /* 0x4BA */ s16 field_4BA;
    /* 0x4BC */ s32 field_4BC;
    /* 0x4C0 */ s32 field_4C0;
} Actor443500Work;
STATIC_ASSERT_SIZEOF(Actor443500Work, 0x4C4);

/// Argument block of message 0x7D4: a world translation followed by the Euler
/// angles handed to `RotMatrix`.
typedef struct Actor443500PlaceArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor443500PlaceArgs;
STATIC_ASSERT_SIZEOF(Actor443500PlaceArgs, 0x18);

/// `GsCOORDINATE2` at `TmdObject::coords`, with the Euler angles kept in the
/// slot libgs names `param` (0x44), from which the rotation is rebuilt.
typedef struct Actor443500Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor443500Coord;
STATIC_ASSERT_SIZEOF(Actor443500Coord, 0x4C);

/// Position triple in this actor's layout table; 8 bytes with a trailing pad.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 pad;
} Actor443500Vec;
STATIC_ASSERT_SIZEOF(Actor443500Vec, 8);

/// 12-byte layout record copied verbatim by `func_actor_443500_80132A68`.
typedef struct {
    s8 b[12];
} Actor443500Blob;
STATIC_ASSERT_SIZEOF(Actor443500Blob, 0xC);

/// Table of pointers into the layout data: the two-entry vector list at
/// `field_4`, the six-entry list at `field_8` and the two 12-byte records at
/// `field_C`. `D_actor_443500_801587D8` is the template, `D_8018828C` the live
/// copy in the room overlay slot.
typedef struct {
    s32              field_0;
    Actor443500Vec*  field_4;
    Actor443500Vec*  field_8;
    Actor443500Blob* field_C;
    void*            field_10;
} Actor443500Layout;
STATIC_ASSERT_SIZEOF(Actor443500Layout, 0x14);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_443500_80132078(Task* task);
void func_actor_443500_801321F0(Task* task);
void func_actor_443500_80132594(Task* task);
void func_actor_443500_801326A0(Task* task);
void func_actor_443500_801327A4(Task* arg0);
void func_actor_443500_801327C4(Task* task);
s32  func_actor_443500_801327E0(Task* task, s32 anim, GpAnimArg* params, s32 arg3);
s32  func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3);
void func_actor_443500_80132A68(s32 arg0);

/// State table of the actor's child task (`TaskDesc` entry 1): setup, the
/// per-frame flag mirror and `taskKill`.
const TaskFuncTable3 D_actor_443500_80131E24 = {
    { func_actor_443500_80132594, func_actor_443500_801326A0, taskKill }
};

/// State table of the actor's main task (`TaskDesc` entry 0): the spawn
/// handler, the per-frame tick and the exit callback.
const TaskFuncTable3 D_actor_443500_80131E30 = {
    { func_actor_443500_80132078, func_actor_443500_801321F0, func_actor_443500_801327A4 }
};

extern TaskDesc D_80187618;
extern TaskDesc D_actor_443500_80140E38;

extern GpAreaApplyRec D_8018A638;

extern s8 D_8007272D;

/// Global mode byte in the main executable; while it is nonzero the main task
/// skips its state handler.
extern u8 D_801153F4;

/// Default animation arguments, 0x14 bytes: `{ NULL, 0x1C, 1, 4, 0 }`.
extern GpAnimArg D_actor_443500_80158728;

/// The actor's two-entry `TaskDesc` table; the spawn handler starts entry 1.
extern TaskDesc D_actor_443500_8015873C;

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::msgTable` (0x24).
extern s32 D_actor_443500_80158754;

/// The bank table `func_actor_443500_801327E0` re-seeds the work block's slot
/// array off: one entry, the animation bank the default preset's `field_0` of
/// zero selects.
extern void* D_actor_443500_80158724[];

extern Actor443500Layout D_8018828C;
extern Actor443500Layout D_actor_443500_801587D8;

void func_actor_443500_80131E3C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x240, 0x100);
        return;
    }
    Gp_ResetCap();
}

void func_actor_443500_80131E84(s32 arg0)
{
    if (GameFlag_GetNibble(0xDF) > 0) {
        if (arg0 != 0) {
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            return;
        }
        Gp_ResetCap();
    }
}

void func_actor_443500_80131EE4(void)
{
    Gp_RunCapCmd(GameFlag_GetNibble(0xDF) == 0 ? 6 : 9, 0);
}

void func_actor_443500_80131F18(void)
{
    Task_SpawnFromTable(&D_80187618, 0, 1, 0);
    Gp_MsgPlayer3F3(0);
    Gp_MsgPlayerWeapon(0);
}

void func_actor_443500_80131F58(void)
{
    Task_SpawnFromTable(&D_actor_443500_80140E38, 0, 0, 0);
}

void func_actor_443500_80131F88(Task* arg0)
{
    s16 temp_v0;
    s32 temp_a0;

    temp_a0 = (((0x1E - arg0->killCountdown) * 0xFF) / 30) & 0xFF;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
    temp_v0             = (u16)arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}

void func_actor_443500_8013201C(s16 arg0)
{
    Gp_StartCapSlot(5, 1, arg0);
}

/// Applies the 0xFF-terminated area record list at `D_8018A638` through
/// `Gp_ApplyAreaRecs`. It is reached only through the function pointers in
/// the actor's data.
void func_actor_443500_80132048(void)
{
    Gp_ApplyAreaRecs(&D_8018A638);
}

void func_actor_443500_8013206C(s8 arg0)
{
    D_8007272D = arg0;
}

/// Spawn handler: allocates the work block, seeds its head from the parent
/// model, starts the actor's child task and copies the location it spawns over
/// from the session key onto that child's model, then installs the animation
/// table, the exit callback and the tick handler.
void func_actor_443500_80132078(Task* task)
{
    Actor443500Work* work;
    GpAreaKey        key;
    GpAreaKey*       sessionKey;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     entry;
    TmdObject*       model;
    Task*            spawned;
    s32              idx;
    u32              raw;

    work = memCalloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_4BC = -1;
    work->field_4C0 = ((TmdObject*)task->extra)->flags;
    spawned         = Task_SpawnFromTable(&D_actor_443500_8015873C, 1, 4, (s32)task);
    if (spawned != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = ((Actor443500Ctx*)task->spawnArg2)->field_8;
        model      = (TmdObject*)spawned->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = sessionKey->view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_443500_8013297C(task, 0x7D5, 0, 0);
    func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
    func_actor_443500_801327C4(task);
    task->msgTable     = &D_actor_443500_80158754;
    task->exitCallback = func_actor_443500_801327A4;
    task->state++;
}

/// Per-frame tick: while the view is live and idle, views 0..3 hide the model
/// (saving `TmdObject::flags` into `field_4C0`) and views 4..5 restore that
/// saved word, showing the model through message 0x7D5 when flag 0x83 is set.
/// Ticks animation slots 1..0x13 once `field_474` is latched, restarting 0x7D3
/// when slot 1 reports the clip ended. The `field_475 == 0x1C` path is the
/// default clip's sound: `field_4BA` counts to 0xF for a Type6 (views 4/5) or
/// Type7 (view 3) cue, TypeA otherwise while the view is ready, and resets on
/// slot 1's control-entry bit. A visible model gets a ground shadow and a
/// rebuilt child-part matrix; `field_4BC` then counts down to free the buffers.
void func_actor_443500_801321F0(Task* task)
{
    Actor443500Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    s32              i;
    u8               view;

    extra = (TmdObject*)task->extra;
    work  = (Actor443500Work*)task->work;
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->at4.loc.view;
        if (view < 4) {
            work->field_4C0 = extra->flags;
            extra->flags    = extra->flags | 0x80;
        } else if (view < 6) {
            if (GameFlag_GetNibble(0x83) > 0) {
                func_actor_443500_80132A68(0);
                func_actor_443500_8013297C(task, 0x7D5, 1, 0);
            }
            extra->flags = work->field_4C0;
        }
    }
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        if (gGameSession->eventState == 0 && (work->slots[1].flags & 1)) {
            func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
        }
    }
    if (work->field_475 == 0x1C) {
        work->field_4BA++;
        if (work->field_4BA == 0xF) {
            switch (gGameSession->at4.loc.view) {
                case 5:
                    SndEvt_EnqueueType6(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        } else if (gGameSession->viewReady != 0) {
            switch (gGameSession->at4.loc.view) {
                case 5:
                    SndEvt_EnqueueTypeA(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueTypeA(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        }
        if (work->slots[1].flags & 2) {
            work->field_4BA = 0;
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
        if (gGameSession->viewReady != 0) {
            Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
            func_800D7A9C(extra, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        }
    }
    if (work->field_4BC >= 0) {
        if (work->field_4BC == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4BC--;
    }
}

/// Dispatcher of the actor's child task: runs its current state handler from
/// `D_actor_443500_80131E24`, copying the table onto the stack before the call.
void func_actor_443500_8013253C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E24;
    sp.funcs[task->state](task);
}

/// Setup state of the actor's child task (first entry of
/// `D_actor_443500_80131E24`): hides the child's model, then mirrors the
/// parent's (`spawnArg2`) hidden bit 0x80 and buffers-live bit 0x4 as the
/// per-frame state does. It draws the model at order-table offset -2, hangs
/// the child's root coordinate off the parent's part `spawnArg1`, shares the
/// parent's light and colour matrices, reparents the task under the parent and
/// steps to the next state.
void func_actor_443500_80132594(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Per-frame state of the actor's child task: mirrors the hidden bit 0x80 and
/// the buffers-live bit 0x4 of the parent's `TmdObject` - the task the spawn
/// handler passed as `Task::spawnArg2` - onto the child's own model. When the
/// parent's 0x4 is clear the child's is cleared too and its buffers are
/// reallocated through `Tmd_AllocBuffers`.
void func_actor_443500_801326A0(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_443500_80131E30`, skipping the frame while `D_801153F4` is
/// set.
void func_actor_443500_80132738(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E30;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback the spawn handler installs, and the third state of the main
/// task: hands the task to `Gp_EnemyTaskExit`.
void func_actor_443500_801327A4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the model's `TmdObject::lightMtx` / `colorMtx` at the work block's
/// own `light` / `color` matrices, so the actor draws with its own lighting.
void func_actor_443500_801327C4(Task* task)
{
    TmdObject*       ext;
    Actor443500Work* work;

    ext           = task->extra;
    work          = (Actor443500Work*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Animation preset handler of message 0x7D3: when the preset's bank index
/// changes the slot array is re-seeded off bank table `D_actor_443500_80158724` through
/// `func_800B3F84`, then the preset's `field_4` is latched into `field_475` and
/// every slot 1..0x13 is either started -- through `func_800B4114`, the path
/// `field_8` selects and the only one that reads `field_C`, taken only once
/// `field_474` has been raised -- or cleared through `Gp_AnimResetSlot`; either
/// way all of them are advanced once by `Gp_AnimTickIndex`. The trailing store
/// raises the `field_474` latch the start branch above reads and clears
/// `field_4BA`.
s32 func_actor_443500_801327E0(Task* task, s32 anim, GpAnimArg* params, s32 arg3)
{
    Actor443500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor443500Work*)task->work;
    ext  = task->extra;
    if (params->field_0 != work->field_476) {
        work->field_476 = params->field_0;
        func_800B3F84(&work->anim, D_actor_443500_80158724[work->field_476], ext, work->field_334,
                      work->slots);
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
    work->field_4BA = 0;
    return 0;
}

/// Message-0x7D4 handler: places the actor at `args` - the translation goes
/// straight into the root coordinate's local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which the rotation is rebuilt. Clearing `flg`
/// has the world matrix recomputed. Returns 0.
s32 func_actor_443500_80132900(Task* task, s32 arg1, Actor443500PlaceArgs* args)
{
    Actor443500Coord* coord;

    coord             = (Actor443500Coord*)((TmdObject*)task->extra)->coords;
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

/// Message-0x7D5 handler: the four-way switch on `mode` over the `TmdObject`
/// parked in `Task::extra`. `mode` drives `TmdObject::flags`: bit 0x80 marks
/// the actor hidden and bit 0x4 the display buffers being live.
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, latch `mode` in the work block's `field_4BC`, raise 0x4
///   mode 3  show, raise 0x4
///
/// Any other mode returns 1; the four known ones return 0. Either way the
/// resulting flags are mirrored onto `Actor443500Work::field_4C0`, the slot
/// the spawn handler seeds from the model's own flags. `field_4BC` is the word
/// the spawn handler seeds to -1 and the tick counts down to free the buffers.
/// `anim` and `arg3` are unused -- the dispatch passes four arguments.
s32 func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    s32              ret;
    Actor443500Work* work;

    obj  = task->extra;
    work = (Actor443500Work*)task->work;
    ret  = 0;
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
            obj->flags     |= 0x80;
            work->field_4BC = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    work->field_4C0 = obj->flags;
    return ret;
}

/// Copy the overlay's layout template `D_actor_443500_801587D8` into the live
/// table at `D_8018828C`. When `arg0` is nonzero, shift the six live target
/// positions by `(0, 0x7D0, 0)` afterwards. The per-frame tick calls this
/// with 0 before showing the model.
void func_actor_443500_80132A68(s32 arg0)
{
    Actor443500Layout* dst;
    Actor443500Layout* src;
    Actor443500Vec     d;
    s32                i;

    dst = &D_8018828C;
    src = &D_actor_443500_801587D8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
    } else {
        d.x = 0;
        d.y = 0x7D0;
    }
    d.z = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}
