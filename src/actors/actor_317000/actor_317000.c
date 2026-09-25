#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the overlay's walker, allocated zeroed by its spawn routine
/// and kept at `Task::work`: a nineteen-part rig and the model state, whose
/// matrices the model is lit with, then the walk, which this actor runs
/// without an arrival threshold.
typedef struct Actor317000Work {
    ActorAnimRig19   rig;
    ActorModelState  model;
    ActorWalkState   walk;
    /* 0x4C4 */ s8   field_4C4;
    /* 0x4C5 */ s8   field_4C5;
    /* 0x4C6 */ s16  field_4C6;
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ byte pad_4CA[0x2];
} Actor317000Work;
STATIC_ASSERT_SIZEOF(Actor317000Work, 0x4CC);

/// Optional start animation for the same handler: the preset's `field_4` and
/// the `model.nextAnimId` byte. Absent, the defaults are anim 2 and 1.
typedef struct Actor317000SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor317000SpawnAnim;

/// Indexed by `Actor317000Work::model.bank` for `func_800B3F84`'s second
/// argument by `func_actor_317000_80162458` and `func_actor_317000_80162A10`.
/// Every preset the actor builds has `field_0` 0, so only the first word is
/// ever read; the words after it (among them the address of
/// `func_actor_317000_80162624`) suggest a larger record, not a bank array.
extern void* D_actor_317000_8016CF40[];

/// `func_800B4114` is not declared in `gameplay/1BC.h`; its callers here pass
/// the animation id as a signed value.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_317000_8016267C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_317000_8016CF50[];

void func_actor_317000_80161E68(Task* task);
void func_actor_317000_801620BC(Task* task);
void func_actor_317000_801621F4(Task* task, Task* targetTask, s32 arg2, s32 arg3, s32 arg4);
void func_actor_317000_8016267C(Task* arg0);
void func_actor_317000_80162724(Task* arg0);
void func_actor_317000_80162744(Task* arg0);
void func_actor_317000_80162760(Task* arg0);
void func_actor_317000_80162768(Task* arg0);
void func_actor_317000_801627D0(Task* arg0);
void func_actor_317000_801628D8(Task* task);
void func_actor_317000_80162950(Task* arg0);
s32  func_actor_317000_80162A10(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3);
s32  func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3);

/// The actor's three task states, which `func_actor_317000_80162624` runs by
/// `Task::state`: spawn, per-frame tick and exit.
const TaskFuncTable3 D_actor_317000_80161E24 = { {
    func_actor_317000_8016267C,
    func_actor_317000_80161E68,
    func_actor_317000_80162724,
} };

/// The four step handlers `func_actor_317000_80162768` runs by
/// `Actor317000Work::walk.motionStep`.
const TaskFuncTable4 D_actor_317000_80161E30 = { {
    func_actor_317000_801627D0,
    func_actor_317000_801628D8,
    func_actor_317000_80162950,
    func_actor_317000_801620BC,
} };

/// The constant local-space offset `func_actor_317000_801628D8` rotates
/// through the root coordinate into `Actor317000Work::step`.
const VECTOR D_actor_317000_80161E40 = { 0, 0xFF800000, 0x400000, 0 };

/// Per-frame tick. Runs the state body `Actor317000Work::walk.motion` selects
/// from a two-entry stack table, then integrates the 16.16 position: `step` is
/// added to `walk.acc`, `step.vy` gains 0x120000 while `field_4C4` is raised, the
/// integer halves move the root coordinate and only the fractions are kept.
/// The animation slots tick, the second coordinate is refreshed while
/// `gGameSession->viewReady` is set, `field_4C6` ramps up by 0x40 to 0x1000 or
/// down by 0x80 to 0 on `field_4C5`, and the aim body runs against slot 3.
/// A non-negative `field_4C8` counts down and frees the model buffers at 0.
void func_actor_317000_80161E68(Task* task)
{
    TmdObject*       ext                 = task->extra;
    Actor317000Work* work                = (Actor317000Work*)task->work;
    void             (*states[2])(Task*) = { func_actor_317000_80162760, func_actor_317000_80162768 };
    GsCOORDINATE2*   coord;
    s32              i;

    states[(s16)work->walk.motion](task);

    coord                = ((TmdObject*)task->extra)->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    if (work->field_4C4 != 0) {
        work->walk.step.vy += 0x120000;
    }
    coord->coord.t[0]  += work->walk.acc[0].h.hi;
    coord->coord.t[1]  += work->walk.acc[1].h.hi;
    coord->coord.t[2]  += work->walk.acc[2].h.hi;
    coord->flg          = 0;
    work->walk.acc[0].w = work->walk.acc[0].h.lo;
    work->walk.acc[1].w = work->walk.acc[1].h.lo;
    work->walk.acc[2].w = work->walk.acc[2].h.lo;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)&((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_4C5 != 0) {
        work->field_4C6 += 0x40;
        if (work->field_4C6 > 0x1000) {
            work->field_4C6 = 0x1000;
        }
    } else {
        work->field_4C6 -= 0x80;
        if (work->field_4C6 < 0) {
            work->field_4C6 = 0;
        }
    }
    func_actor_317000_801621F4(task, gameGetPtrSlot(3), 0x400, 0x200, work->field_4C6);
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C8--;
    }
}

/// Step handler at index 3 of `D_actor_317000_80161E30`. The actor's own coordinate and the `gameGetPtrSlot(3)` task's
/// (the player) are normalised into `dir`, whose yaw `ratan2` takes over
/// `dir.vz`, and the result is written as the roll/pitch-free facing
/// `{ 0, yaw, 0 }` at `GpCoordExt::param.rot`. The same yaw is then compared
/// against the yaw `Gp_ExtractEuler` reads back out of the node's own matrix:
/// when the two are within 0x40 (64 of 4096 units) the actor is facing its
/// target already, which clears the work's dispatch index and its companion
/// halfword; otherwise the matrix's yaw is stepped toward the target by that
/// same 0x40 and `RotMatrix` rebuilds the node from the adjusted angles.
void func_actor_317000_801620BC(Task* task)
{
    Actor317000Work* work;
    GpCoordExt*      coord;
    GpCoordExt*      target;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;
    SVECTOR          ang;
    s16              diff;
    s32              absDiff;
    s32              y;

    coord  = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    target = (GpCoordExt*)((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    work   = (Actor317000Work*)task->work;

    delta.vx = target->coord.t[0] - coord->coord.t[0];
    delta.vy = target->coord.t[1] - coord->coord.t[1];
    delta.vz = target->coord.t[2] - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;

    Gp_ExtractEuler(&ang, &coord->coord);
    diff    = ratan2(dir.vx, dir.vz) - ang.vy;
    absDiff = abs(diff);
    if (absDiff >= 0x41) {
        y = ang.vy;
        if (diff < 0) {
            ang.vy = y - 0x40;
        } else {
            ang.vy = y + 0x40;
        }
    } else {
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }
    RotMatrix(&ang, &coord->coord);
    coord->flg = 0;
}

/// Aim body the per-frame tick `func_actor_317000_80161E68` calls with
/// `gameGetPtrSlot(3)` (the player) as `targetTask`; it never reads the three
/// arguments after it (0x400, 0x200 and `Actor317000Work::field_4C6`). The delta from the actor's sixth coordinate
/// (`coord[5]`) to the target's fifth is normalised, taken through the
/// transpose of the actor's third coordinate's `workm`, and normalised again
/// into `dir`. The three `ratan2`s reduce `dir` to an Euler triple -- the YZ,
/// XZ and XY plane angles, the middle one against the negated magnitude of
/// `dir.vz` -- which is added halfword-wise to the euler `Gp_ExtractEuler`
/// reads out of `coord[5]` before `RotMatrix` rebuilds it. An eighth of the
/// rebuilt euler then offsets `coord[3]`'s own euler, and `coord[5]` is
/// re-read and scaled by 5/8 before being rewritten as the identity rotated
/// by it.
///
/// `coord[5]` is kept as its own `head` pointer and `coord[3].coord` as its
/// own `arm` matrix rather than indexed off `coord`: the second and third
/// reads of `task->extra->coords` are re-derived rather than sharing the
/// first, which is what keeps the task argument live in `$s1` across the
/// calls and puts `&target[4]` in its own register.
void func_actor_317000_801621F4(Task* task, Task* targetTask, s32 arg2, s32 arg3, s32 arg4)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    GsCOORDINATE2* head;
    GsCOORDINATE2* aim;
    MATRIX*        arm;
    GpMtxWords*    words;
    VECTOR         delta;
    VECTOR         dir;
    SVECTOR        ang;
    SVECTOR        vec;
    SVECTOR        rot;

    coord  = ((TmdObject*)task->extra)->coords;
    target = ((TmdObject*)targetTask->extra)->coords;
    head   = &coord[5];
    aim    = &target[4];

    delta.vx = aim->workm.t[0] - head->workm.t[0];
    delta.vy = aim->workm.t[1] - head->workm.t[1];
    delta.vz = aim->workm.t[2] - head->workm.t[2];
    VectorNormal(&delta, &delta);
    ApplyTransposeMatrixLV(&((TmdObject*)task->extra)->coords[2].workm, &delta, &delta);
    VectorNormal(&delta, &dir);

    rot.vx = ratan2(dir.vz, dir.vy);
    rot.vy = ratan2(dir.vx, -ABS(dir.vz));
    rot.vz = ratan2(dir.vx, dir.vy);

    Gp_ExtractEuler(&ang, &coord[5].coord);
    ang.vx = (u16)ang.vx + (u16)rot.vx;
    ang.vy = (u16)ang.vy + (u16)rot.vy;
    ang.vz = (u16)ang.vz + (u16)rot.vz;
    RotMatrix(&ang, &coord[5].coord);

    Gp_ExtractEuler(&rot, &coord[5].coord);
    arm = &((TmdObject*)task->extra)->coords[3].coord;
    Gp_ExtractEuler(&vec, arm);
    vec.vx = (u16)vec.vx + rot.vx / 8;
    vec.vy = (u16)vec.vy + rot.vy / 8;
    vec.vz = (u16)vec.vz + rot.vz / 8;
    RotMatrix(&vec, arm);

    Gp_ExtractEuler(&vec, &coord[5].coord);
    vec.vx = vec.vx * 5 / 8;
    vec.vy = vec.vy * 5 / 8;
    vec.vz = vec.vz * 5 / 8;

    words          = (GpMtxWords*)&coord[5].coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord[5].coord);
}

/// Message 0x7DD handler of the table `func_actor_317000_8016267C` installs,
/// and the actor's spawn body. The placement's position and rotation are
/// copied into the work's `target` and `field_4B8..field_4BC`, `walk.motion` --
/// the index `func_actor_317000_80161E68` dispatches on -- is latched to 1, and
/// the animation preset is filled: `field_0` 0, `field_4` the optional start
/// animation (2 when absent) with `model.nextAnimId` taking its companion byte (1 when
/// absent), `field_8` 1, `field_C` 5 and `field_10` 1.
///
/// The preset is then installed the way `func_actor_317000_80162A10` installs
/// one, written out in-line: a changed `field_0` resets the bank in
/// `D_actor_317000_8016CF40` through `func_800B3F84` (`model.bank` latches it,
/// `model.animId` goes back to -1), and a changed `field_4` -- or a preset asking
/// for slots when `model.ticking` says the slots are already ticking -- is pushed
/// onto `func_800B4114`'s per-slot loop instead of the `Gp_AnimResetSlot`
/// one, followed by a `Gp_AnimTickIndex` pass over the same 0x12 slots and
/// `model.ticking` raised. Returns 0 either way.
s32 func_actor_317000_80162458(Task* task, s32 arg1, GpXformArg* place, Actor317000SpawnAnim* anim)
{
    Actor317000Work* work;
    Actor317000Work* w;
    GpAnimArg        preset;
    GpAnimArg*       msg;
    s32              i;
    TmdObject*       ext;

    w                      = (Actor317000Work*)task->work;
    w->walk.motion         = 1;
    w->walk.target.vx      = place->pos.vx;
    w->walk.target.vy      = place->pos.vy;
    w->walk.target.vz      = place->pos.vz;
    w->walk.rotX           = place->rot.vx;
    w->walk.rotY           = place->rot.vy;
    w->walk.rotZ           = place->rot.vz;
    preset.animBlock.index = 0;
    if (anim != NULL) {
        preset.field_4      = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        preset.field_4      = 2;
        w->model.nextAnimId = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor317000Work*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank   = msg->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_317000_8016CF40[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->field_4 != work->model.animId) {
        work->model.animId = msg->field_4;
        if (msg->field_8 != 0 && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Task callback of the actor: copies the three-handler table
/// `D_actor_317000_80161E24` (spawn `func_actor_317000_8016267C`, per-frame
/// tick `func_actor_317000_80161E68`, exit `func_actor_317000_80162724`) onto
/// the stack and runs the entry `Task::state` selects.
void func_actor_317000_80162624(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_317000_80161E24;
    sp.funcs[task->state](task);
}

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block every
/// later handler reads through `Task::work`, seeds the three -1 fields and the
/// three cleared words the work's own init expects, republishes the light and
/// colour matrices onto the display object, sets `TmdObject::flags` bit 0x80
/// through the mode-0 call, then installs the message table and the exit
/// handler. An allocation failure ends the task instead of leaving a
/// half-built actor behind.
void func_actor_317000_8016267C(Task* arg0)
{
    Actor317000Work* work;

    work = memCalloc(sizeof(Actor317000Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work          = (TaskIdMap*)work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->field_4C8     = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    func_actor_317000_80162744(arg0);
    func_actor_317000_80162BC4(arg0, 0x7D5, 0, 0);

    arg0->msgTable     = D_actor_317000_8016CF50;
    arg0->exitCallback = func_actor_317000_80162724;
    arg0->state++;
}

/// Exit handler, both the third entry of `D_actor_317000_80161E24` and the
/// `Task::exitCallback` `func_actor_317000_8016267C` installs: ends the task
/// through `Gp_EnemyTaskExit`.
void func_actor_317000_80162724(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the display object's light and colour matrices at the work block's
/// own copies, `Actor317000Work::light` and `color`.
void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*       ext;
    Actor317000Work* work;

    ext           = arg0->extra;
    work          = (Actor317000Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Index 0 of the two-entry table `func_actor_317000_80161E68` dispatches on
/// `Actor317000Work::walk.motion`: does nothing.
void func_actor_317000_80162760(Task* arg0)
{
}

/// Index 1 of the two-entry table `func_actor_317000_80161E68` dispatches on
/// `Actor317000Work::walk.motion`: copies the four step handlers
/// `D_actor_317000_80161E30` onto the stack and runs the one
/// `Actor317000Work::walk.motionStep` selects, read sign-extended.
void func_actor_317000_80162768(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor317000Work* work;

    work = (Actor317000Work*)arg0->work;
    sp   = D_actor_317000_80161E30;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// Step handler at index 0 of `D_actor_317000_80161E30`: Euler-extracts the
/// root coordinate into `vec`, and while the yaw gap to the target
/// `work->walk.rotY` is at least 0x41 it steps `vec.vy` toward it by 0x40 --
/// the step is taken on an `s32` widening of the extracted yaw -- and
/// otherwise snaps the yaw to the target and plays anim 0x7D3 with a preset
/// whose `field_4` is the literal 2, clearing the `field_4C4` flag and
/// advancing `walk.motionStep`. Either way the root coordinate is rebuilt as the
/// identity matrix rotated by `vec`.
void func_actor_317000_801627D0(Task* arg0)
{
    Actor317000Work* work;
    GpMtxWords*      words;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    GpAnimArg        preset;
    s32              vy;
    s16              diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor317000Work*)arg0->work;

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
        preset.field_4         = 2;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_317000_80162A10(arg0, 0x7D3, &preset, 0);
        work->field_4C4 = 0;
        work->walk.motionStep++;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Step handler at index 1 of `D_actor_317000_80161E30`, reached by the
/// `walk.motionStep` advance `func_actor_317000_801627D0` ends with: rotates the constant local-space
/// offset `D_actor_317000_80161E40` through the root part's matrix into
/// `work->walk.step`, then raises the flag at 0x4C4 and moves the dispatcher on.
void func_actor_317000_801628D8(Task* task)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor317000Work*)task->work;

    vec = D_actor_317000_80161E40;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->field_4C4 = 1;
    work->walk.motionStep++;
}

/// Step handler at index 2 of `D_actor_317000_80161E30`, the step after
/// `func_actor_317000_801628D8` and reached by the `walk.motionStep` advance that
/// body ends with. While the root coordinate's Y is below -0x30 it does
/// nothing; above it the rise is over: the local-space `work->walk.step` the
/// previous body wrote is cleared, the animation is re-applied through message
/// 0x7D3 with the latched `model.nextAnimId` state, and sound 0x400A000B is queued
/// panned and attenuated from the root coordinate's matrix. The `field_4C4`
/// flag the previous body raised is cleared and the dispatcher advances again.
void func_actor_317000_80162950(Task* arg0)
{
    GsCOORDINATE2*   coord;
    Actor317000Work* work;
    GpAnimArg        preset;
    s32              pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor317000Work*)arg0->work;
    if (coord->coord.t[1] < -0x30) {
        return;
    }
    preset.animBlock.index = 0;
    preset.field_4         = work->model.nextAnimId;
    preset.field_8         = 1;
    preset.field_C         = 5;
    preset.field_10        = 0;
    func_actor_317000_80162A10(arg0, 0x7D3, &preset, 0);
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x400A000B, pan, (s8)gpGetObjDepth(coord));

    work->walk.step.vx = 0;
    work->walk.step.vy = 0;
    work->walk.step.vz = 0;
    work->field_4C4    = 0;
    work->walk.motionStep++;
}

/// Message 0x7D3 handler of `D_actor_317000_8016CF50`, also called directly by
/// the step handlers `func_actor_317000_801627D0` and
/// `func_actor_317000_80162950` with presets of their own. A preset `field_0`
/// that differs from `Actor317000Work::model.bank` latches it, forgets the
/// current animation (`model.animId` = -1) and re-seeds the slots through
/// `func_800B3F84` from `D_actor_317000_8016CF40[model.bank]`. A `field_4` that
/// differs from `model.animId` latches it and installs it on slots 1..0x12 --
/// through `func_800B4114` with `field_C` when the preset's `field_8` is set and
/// `model.ticking` says the slots are already ticking, through `Gp_AnimResetSlot`
/// otherwise -- then ticks each slot once and raises `model.ticking`. Returns 0.
s32 func_actor_317000_80162A10(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor317000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor317000Work*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank   = msg->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_317000_8016CF40[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    if (msg->field_4 != work->model.animId) {
        work->model.animId = msg->field_4;
        if (msg->field_8 != 0 && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Message 0x7D4 handler of `D_actor_317000_8016CF50`: writes the payload's
/// position into the root coordinate's translation and its Euler angles into
/// `GpCoordExt::param.rot`, rebuilds the rotation from them with `RotMatrix`
/// and clears `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_317000_80162B48(Task* task, s32 arg1, GpXformArg* args)
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

/// Message 0x7D5 handler of `D_actor_317000_8016CF50`, also called directly by
/// the spawn state `func_actor_317000_8016267C` with mode 0. `mode` sets or
/// clears bit 0x80 of `TmdObject::flags` and sets or clears bit 0x4:
///
///   mode 0  set 0x80, clear 0x4
///   mode 1  clear 0x80, `Tmd_AllocBuffers`, clear 0x4
///   mode 2  set 0x80, store 2 in the countdown `Actor317000Work::field_4C8`
///           that `func_actor_317000_80161E68` ends in `Tmd_FreeBuffers`,
///           set 0x4
///   mode 3  clear 0x80, set 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused.
s32 func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    Actor317000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor317000Work*)task->work;
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
            work->field_4C8 = mode;
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
    return ret;
}

/// Message 0x7DB handler of the table `func_actor_317000_8016267C` installs:
/// latches the payload's halfword at 0x2 into `Actor317000Work::field_4C5` --
/// 0 for mode 0, the mode itself for mode 1 -- and for any other mode dumps the
/// root coordinate's matrix translation (`"pos"`) and the euler angles
/// `Gp_ExtractEuler` derives from its rotation matrix (`"rot"`) through
/// `GPU_printf`, both under the `"%s=(%d,%d,%d)\n"` format. Returns 0
/// either way.
s32 func_actor_317000_80162CA0(Task* task, s32 arg1, GpCmdArg* msg)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    s32              mode;

    work  = (Actor317000Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    mode  = msg->command;
    switch (mode) {
        case 0:
            work->field_4C5 = 0;
            break;
        case 1:
            work->field_4C5 = mode;
            break;
        default:
            GPU_printf("%s=(%d,%d,%d)\n", "pos", coord->coord.t[0], coord->coord.t[1], coord->coord.t[2]);
            Gp_ExtractEuler(&rot, &coord->coord);
            GPU_printf("%s=(%d,%d,%d)\n", "rot", rot.vx, rot.vy, rot.vz);
            break;
    }
    return 0;
}
