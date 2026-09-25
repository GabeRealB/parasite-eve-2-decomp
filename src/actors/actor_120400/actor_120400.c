#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the parent task, allocated zeroed by its spawn routine and
/// kept at `Task::work`: a twenty-part rig, the walk state, and
/// `freeCountdown`, the frames until the model buffers are freed, -1
/// disabling the countdown.
typedef struct Actor120400MainWork {
    ActorAnimRig20 rig;
    ActorWalkState walk;
    byte           pad_4FC[0x4];
    s16            freeCountdown;
    byte           pad_502[0x2];
} Actor120400MainWork;
STATIC_ASSERT_SIZEOF(Actor120400MainWork, 0x504);

/// Animation source indexed by the bank id the presets latch:
/// `D_actor_120400_8013E744[work->walk.bank]` is the bank handed to
/// `func_800B3F84`.
extern void* D_actor_120400_8013E744[];

/// Optional start animation for `func_actor_120400_80132398`: the preset's
/// `field_4` and the `walk.preset` byte. Absent, the defaults are 0x10 and 1.
typedef struct Actor120400SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor120400SpawnAnim;
STATIC_ASSERT_SIZEOF(Actor120400SpawnAnim, 0x8);

/// The task table the parent is spawned from and its two children are spawned
/// from (entries 1 and 2), and the message table the parent points its
/// `Task::msgTable` at; both live in this overlay's trailing data.
extern TaskDesc   D_actor_120400_8013E748;
extern GpMsgEntry D_actor_120400_8013E76C[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_120400_80131E5C(Task* arg0);
void func_actor_120400_80132050(Task* arg0);
void func_actor_120400_80132254(Task* arg0);
void func_actor_120400_801325A4(Task* task);
void func_actor_120400_801326B0(Task* task);
void func_actor_120400_801327B4(Task* task);
void func_actor_120400_801327D4(Task* task);
void func_actor_120400_801327F0(Task* arg0);
void func_actor_120400_801327F8(Task* task);
void func_actor_120400_80132860(Task* task);
void func_actor_120400_80132920(Task* task);
void func_actor_120400_801329A0(Task* arg0);
s32  func_actor_120400_80132AA0(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3);

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `func_actor_120400_8013254C`.
const TaskFuncTable3 D_actor_120400_80131E24 = { {
    func_actor_120400_801325A4,
    func_actor_120400_801326B0,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent task, dispatched by
/// `func_actor_120400_80132748`.
const TaskFuncTable3 D_actor_120400_80131E30 = { {
    func_actor_120400_80131E5C,
    func_actor_120400_80132050,
    func_actor_120400_801327B4,
} };

/// Steps of the parent's walk sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
const TaskFuncTable4 D_actor_120400_80131E3C = { {
    func_actor_120400_80132860,
    func_actor_120400_80132920,
    func_actor_120400_80132254,
    func_actor_120400_801329A0,
} };

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
const VECTOR D_actor_120400_80131E4C = { 0, 0, 0x200000, 0 };

/// The parent's spawn handler. Allocates the 0x504 `Actor120400MainWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::tpage` / `clut` loaded with the texture page and CLUT
/// row of the `GpAreaPlace` that entry selects, reached through the area key
/// `&gGameSession->at4.loc.view` and indexed by the model id the child's own
/// `spawnArg2` carries at `GpEnemy::placeKey >> 12`, and each then has its
/// texture stream processed twice when it has a buffer. The body ends by
/// pointing the parent's model at its light/colour matrices
/// (`func_actor_120400_801327D4`), pointing `msgTable` at the message table and
/// installing `func_actor_120400_801327B4` as its exit callback.
void func_actor_120400_80131E5C(Task* arg0)
{
    Actor120400MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor120400MainWork*)memCalloc(0x504, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work          = (TaskIdMap*)work;
    work->walk.animId   = -1;
    work->walk.bank     = -1;
    work->freeCountdown = -1;
    work->walk.acc.vx   = 0;
    work->walk.acc.vy   = 0;
    work->walk.acc.vz   = 0;
    spawned             = Task_SpawnFromTable(&D_actor_120400_8013E748, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model      = (TmdObject*)spawned->extra;
        idx        = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_120400_8013E748, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model = (TmdObject*)spawned->extra;
        idx   = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        /* Re-derived address, not the block-1 form: with `sessionKey->field_0`
           for the last byte, global CSE merges this block's area key with the
           one above into a single cross-block pseudo, and the allocation of
           `spawned` and every address temp after it shifts. */
        sessionKey = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->at4.loc.view);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = ((GpAreaKey*)keyAddr)->room;
        key.view   = ((GpAreaKey*)(&gGameSession->at4.loc.view))->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_120400_801327D4(arg0);
    arg0->msgTable     = D_actor_120400_8013E76C;
    arg0->exitCallback = func_actor_120400_801327B4;
    arg0->state       += 1;
}

/// The parent's per-frame update: the motion handler -- entry `walk.motion` of
/// the pair `{func_actor_120400_801327F0, func_actor_120400_801327F8}`, idle or
/// the walk sequence -- runs first, then
/// the three 16.16 step accumulators at 0x4D8..0x4E0 take this frame's `step`,
/// their integer halves are added onto the root coordinate's translation and
/// the fraction is dropped, and `flg` is cleared so the tree rebuilds. With
/// `walk.ticking` set every animation slot is ticked. Unless the model is hidden
/// (bit 0x80 of `TmdObject::flags`), the second coordinate's work matrix
/// feeds `func_800EA1A8` and a non-zero result draws the ground-effect quad;
/// when `gGameSession->viewReady` is set the same coordinate is flagged stale,
/// updated and re-ranked through `func_800D7A9C`. The body ends decrementing
/// the `freeCountdown` teardown timer, freeing the model's buffers on the frame it
/// reaches zero.
void func_actor_120400_80132050(Task* arg0)
{
    TmdObject*           ext      = arg0->extra;
    Actor120400MainWork* work     = (Actor120400MainWork*)arg0->work;
    TaskFunc             funcs[2] = { func_actor_120400_801327F0, func_actor_120400_801327F8 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    s32                  i;

    funcs[work->walk.motion](arg0);
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->walk.acc.vx += work->walk.step.vx;
    work->walk.acc.vy += work->walk.step.vy;
    work->walk.acc.vz += work->walk.step.vz;
    coord->coord.t[0] += (s16)(work->walk.acc.vx >> 16);
    coord->coord.t[1] += (s16)(work->walk.acc.vy >> 16);
    coord->coord.t[2] += (s16)(work->walk.acc.vz >> 16);
    coord->flg         = 0;
    work->walk.acc.vx  = (u16)work->walk.acc.vx;
    work->walk.acc.vy  = (u16)work->walk.acc.vy;
    work->walk.acc.vz  = (u16)work->walk.acc.vz;
    if (work->walk.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// Walk step 2, the arrival check: takes the X/Z distance from the root
/// coordinate to `target`. While it keeps shrinking below `limit` it is stored
/// as the new `limit`; once it no longer does, the target has been reached or
/// passed, so the step plays the preset carrying the `walk.preset` byte through
/// the 0x7D3 handler, stops the velocity `step` and advances `walk.motionStep`.
void func_actor_120400_80132254(Task* arg0)
{
    Actor120400MainWork* work;
    GsCOORDINATE2*       coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    GpAnimArg            preset;

    work  = (Actor120400MainWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.animBlock.index = 0;
        preset.field_4         = work->walk.preset;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_120400_80132AA0(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Message 0x7DD handler of the parent: starts the walk sequence toward a
/// placement. The position and rotation are copied into `target` and
/// `walk.rotX`..`walk.rotZ`, `walk.motion` selects the walk and `walk.motionStep` restarts
/// it, and a start preset is built on the stack -- bank id 0, the optional start
/// animation's id and companion byte (0x10 and 1 when absent), 1, 5 and 1 --
/// and then applied in-line. A changed bank id latches `walk.bank` and reseeds
/// the animation through `func_800B3F84` with the bank this overlay's
/// `D_actor_120400_8013E744` selects; `walk.animId` takes the preset's animation
/// id, and a preset asking for slots while `walk.ticking` says the slots are
/// already ticking is pushed onto `func_800B4114`'s per-slot loop instead of
/// the `Gp_AnimResetSlot` one, followed by a `Gp_AnimTickIndex` pass over the
/// same 0x14 slots and `walk.ticking` raised. Returns 0 either way.
s32 func_actor_120400_80132398(Task* task, s32 arg1, GpXformArg* place, Actor120400SpawnAnim* anim)
{
    Actor120400MainWork* work;
    Actor120400MainWork* w;
    GpAnimArg            preset;
    GpAnimArg*           msg;
    s32                  i;
    TmdObject*           ext;

    w                      = (Actor120400MainWork*)task->work;
    w->walk.motion         = 1;
    w->walk.motionStep     = 0;
    w->walk.target.vx      = place->pos.vx;
    w->walk.target.vy      = place->pos.vy;
    w->walk.target.vz      = place->pos.vz;
    w->walk.rotX           = place->rot.vx;
    w->walk.rotY           = place->rot.vy;
    w->walk.rotZ           = place->rot.vz;
    preset.animBlock.index = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->walk.preset = anim->field_4;
    } else {
        preset.field_4 = 0x10;
        w->walk.preset = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor120400MainWork*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->walk.bank) {
        work->walk.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_120400_8013E744[work->walk.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->walk.animId = msg->field_4;
    if (msg->field_8 != 0 && work->walk.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->walk.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->walk.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->walk.ticking = 1;
    return 0;
}

/// State dispatcher of the two child tasks: copies their spawn/tick/teardown
/// table onto the stack and runs the entry `Task::state` selects.
void func_actor_120400_8013254C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn state of a child task: its model starts hidden and then takes the
/// parent model's bits 0x80 and 0x4 as the tick does, allocating its buffers
/// while bit 0x4 is clear. The model is linked at ordering-table offset -2, its
/// root coordinate hangs off the parent's coordinate `spawnArg1`, it shares
/// the parent's light and colour matrices, and the task is reparented under
/// the parent before the state advances.
void func_actor_120400_801325A4(Task* task)
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

/// Per-frame tick of a child task: copies the parent model's hidden bit (0x80)
/// and bit 0x4 onto the child's own model. While the parent's bit 0x4 is
/// clear the child's display buffers are (re)allocated as well.
void func_actor_120400_801326B0(Task* task)
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

/// State dispatcher of the parent task: copies its spawn/tick/teardown table
/// onto the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_120400_80132748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback of the parent task, installed by its spawn handler: runs the
/// common enemy teardown.
void func_actor_120400_801327B4(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the parent's model at the light and colour matrices held in its own
/// work block.
void func_actor_120400_801327D4(Task* task)
{
    TmdObject*           ext;
    Actor120400MainWork* work;

    ext           = task->extra;
    work          = (Actor120400MainWork*)task->work;
    ext->lightMtx = &work->walk.light;
    ext->colorMtx = &work->walk.color;
}

/// Motion handler 0 of the parent, idle: does nothing.
void func_actor_120400_801327F0(Task* arg0)
{
}

/// Motion handler 1 of the parent, the walk sequence: copies the step table
/// onto the stack and runs the entry `walk.motionStep` selects.
void func_actor_120400_801327F8(Task* task)
{
    Actor120400MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor120400MainWork*)task->work;
    fns  = D_actor_120400_80131E3C;
    fns.funcs[work->walk.motionStep](task);
}

/// Walk step 0: turns the root part to face `target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_120400_80132860(Task* task)
{
    Actor120400MainWork* work;
    GpCoordExt*          coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor120400MainWork*)task->work;
    coord = (GpCoordExt*)((TmdObject*)task->extra)->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    work->walk.motionStep++;
}

/// Walk step 1: rotates the constant forward offset `D_actor_120400_80131E4C`
/// through the root part's matrix into `step`, opens the arrival threshold to
/// 0x7FFF, which disables it, and advances the step.
void func_actor_120400_80132920(Task* task)
{
    Actor120400MainWork* work;
    GsCOORDINATE2*       coord;
    VECTOR               vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor120400MainWork*)task->work;

    vec = D_actor_120400_80131E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// Walk step 3, the turn to the placement yaw: Euler-extracts the root
/// coordinate into `vec`, and while the yaw gap to `walk.rotY` is at least
/// 0x41 steps `vec.vy` toward it by 0x40, taking the step on an `s32` widening
/// of the extracted yaw. Otherwise it snaps the yaw to the target, plays the
/// preset carrying the `walk.preset` byte through the 0x7D3 handler and clears
/// `walk.motion` / `walk.motionStep`, which returns the parent to idle. Either way the
/// root coordinate is rebuilt as the identity matrix rotated by `vec`.
void func_actor_120400_801329A0(Task* arg0)
{
    Actor120400MainWork* work;
    OverlayMatWords*     words;
    GsCOORDINATE2*       coord;
    SVECTOR              vec;
    GpAnimArg            preset;
    s32                  vy;
    s16                  diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor120400MainWork*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                 = work->walk.rotY;
        preset.animBlock.index = 0;
        preset.field_4         = work->walk.preset;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_120400_80132AA0(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (OverlayMatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message 0x7D3 handler of the parent, the animation preset: when the preset
/// names a different bank than the one latched in `walk.bank`, re-seeds the
/// slots from `D_actor_120400_8013E744`; then, with `field_8` set and the slots
/// already live, restarts animation `field_4` on every slot through
/// `func_800B4114` (passing `field_C`), otherwise resets every slot to it, and
/// ticks them all once. Returns 0.
s32 func_actor_120400_80132AA0(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor120400MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor120400MainWork*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->walk.bank) {
        work->walk.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_120400_8013E744[work->walk.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->walk.animId = msg->field_4;
    if (msg->field_8 != 0 && work->walk.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->walk.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->walk.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->walk.ticking = 1;
    return 0;
}

/// Message 0x7D4 handler of the parent: places the root part at the message's
/// position and Euler angles, rebuilding the rotation from them and clearing
/// `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_120400_80132BBC(Task* task, s32 arg1, GpXformArg* args)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Message 0x7D5 handler of the parent: shows or hides its model. `mode`
/// drives the `TmdObject` parked in `Task::extra` -- bit 0x80 hides it, bit
/// 0x4 is the one the children copy alongside it:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, start the `freeCountdown` countdown to freeing the buffers, raise 0x4
///   mode 3  show, raise 0x4
///
/// Returns 0 for the four known modes and 1 for any other.
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*           obj;
    Actor120400MainWork* work;
    s32                  ret;

    obj  = task->extra;
    work = (Actor120400MainWork*)task->work;
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
            obj->flags         |= 0x80;
            work->freeCountdown = mode;
            obj->flags         |= 4;
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

/// Message 0x7DB handler of the parent: ignores the message and returns 0.
s32 func_actor_120400_80132D14(void)
{
    return 0;
}
