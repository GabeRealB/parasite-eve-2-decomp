#include "common.h"

#include <psyq/libgte.h>
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

/// 0x14-byte animation preset the two 0x7D3 handlers take:
/// `func_actor_350700_80162860` for the enemy actor, `func_actor_350700_801636A8`
/// for the parent. `field_0` is the bank index, `field_4` the animation id,
/// `field_8` selects a blended restart once the slots run and `field_C` is
/// handed to that restart. The approach, arrival and turn steps build one on
/// their own stack.
typedef struct Actor350700AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor350700AnimPreset;
STATIC_ASSERT_SIZEOF(Actor350700AnimPreset, 0x14);

/// Spawn placement `func_actor_350700_801621B4` copies into the work block:
/// the position into `Actor350500Work::target`, the rotation into
/// `field_4B8..field_4BC`.
typedef struct Actor350700Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor350700Placement;
STATIC_ASSERT_SIZEOF(Actor350700Placement, 0x18);

/// Optional start animation for the same handler: the preset's `field_4`
/// and the `field_43F` byte. Absent, the defaults are anim 3 (or 2 once
/// `field_4C4` is set) and 1.
typedef struct Actor350700SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor350700SpawnAnim;

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`) holds the Euler angles the two
/// face-the-target steps write and hand straight to `RotMatrix`.
typedef struct Actor350700Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor350700Coord;
STATIC_ASSERT_SIZEOF(Actor350700Coord, 0x4C);

/// Payload of the two-case message handler `func_actor_350700_80162AF4`:
/// only the halfword at 0x2 is read, selecting the variant it latches into
/// `Actor350500Work::field_4C4`.
typedef struct Actor350700Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor350700Msg;
STATIC_ASSERT_SIZEOF(Actor350700Msg, 0x4);

/// Work block allocated by `func_actor_350700_80162B30` (`memCalloc(0x50C)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor350500Work`. This is the parent
/// actor's block: the init seeds the
/// two `sb` bytes at 0x475/0x476 and the word at 0x508 to -1, clears the
/// three words at 0x4D8..0x4E0, and stores the three child tasks it spawns
/// from `D_actor_350700_801708DC` at 0x4FC/0x500/0x504. `func_actor_350700_801633DC`
/// then republishes the light/colour matrix pair onto the parent's
/// `TmdObject::lightMtx` / `field_20`, exactly as `func_actor_350700_801624B4`
/// does for `Actor350500Work`.
///
/// The size is the allocation; the fields below are the ones the parent's
/// handlers touch. The tick keeps a
/// 16.16 accumulator triple at 0x4D8..0x4E0, fed from the deltas in
/// `step`; only each accumulator's high half reaches the root
/// coordinate, and the low half is re-zeroed every frame.
typedef struct Actor350700MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];  // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       poses[0x140]; // pose buffer `func_800B3F84` is handed
    /* 0x474 */ s8         field_474;    // non-zero while the animation slots tick
    /* 0x475 */ s8         field_475;    // current animation id
    /* 0x476 */ s8         field_476;    // current bank index into `D_actor_350700_801708D8`
    /* 0x477 */ s8         field_477;    // preset byte the arrival and turn steps pass as `field_4`
    /* 0x478 */ MATRIX     light;
    /* 0x498 */ MATRIX     color;
    /* 0x4B8 */ VECTOR3    target;    // world position the turn-to-face step steers toward
    /* 0x4C4 */ byte       pad_4C4[0x4];
    /* 0x4C8 */ VECTOR3    step;      // per-frame local-space deltas the accumulators take
    /* 0x4D4 */ byte       pad_4D4[0x4];
    /* 0x4D8 */ s32        field_4D8; // 16.16 accumulators; only the high half reaches the coordinate
    /* 0x4DC */ s32        field_4DC;
    /* 0x4E0 */ s32        field_4E0;
    /* 0x4E4 */ byte       pad_4E4[0x4];
    /* 0x4E8 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4F0 */ u16        field_4F0;
    /* 0x4F2 */ u16        field_4F2; // target yaw the turn-to-face step steers toward
    /* 0x4F4 */ u16        field_4F4;
    /* 0x4F6 */ byte       pad_4F6[0x2];
    /* 0x4F8 */ s16        field_4F8; // selects which of the two handlers the tick runs
    /* 0x4FA */ u16        field_4FA; // index into the step-handler table `D_actor_350700_80161E68`
    /* 0x4FC */ Task*      field_4FC;
    /* 0x500 */ Task*      field_500;
    /* 0x504 */ Task*      field_504;
    /* 0x508 */ s32        field_508;
} Actor350700MainWork;
STATIC_ASSERT_SIZEOF(Actor350700MainWork, 0x50C);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation bank tables of the enemy actor and of the parent block.
extern void* D_actor_350700_80169D0C[];
extern void* D_actor_350700_801708D8[];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350700_80162404`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350700_80169D1C[];

/// The `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks from,
/// and the message table it points the parent's `Task::msgTable` at: ids
/// 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `func_actor_350700_801636A8`, terminated by 0x7FFFFFFF.
extern TaskDesc   D_actor_350700_801708DC;
extern GpMsgEntry D_actor_350700_8017090C[];

void func_actor_350700_80161E88(Task* arg0);
void func_actor_350700_80162070(Task* arg0);
void func_actor_350700_80162404(Task* arg0);
void func_actor_350700_80162494(Task* arg0);
void func_actor_350700_801624B4(Task* arg0);
void func_actor_350700_801624D0(Task* arg0);
void func_actor_350700_801624D8(Task* arg0);
void func_actor_350700_80162540(Task* task);
void func_actor_350700_8016261C(Task* arg0);
void func_actor_350700_80162764(Task* arg0);
s32  func_actor_350700_80162860(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3);
void func_actor_350700_80162B30(Task* arg0);
void func_actor_350700_80162D5C(Task* arg0);
void func_actor_350700_80162F7C(Task* arg0);
void func_actor_350700_801632CC(Task* task);
void func_actor_350700_80163348(Task* task);
void func_actor_350700_801633BC(Task* arg0);
void func_actor_350700_801633DC(Task* task);
void func_actor_350700_801633F8(Task* arg0);
void func_actor_350700_80163400(Task* task);
void func_actor_350700_80163468(Task* task);
void func_actor_350700_80163528(Task* task);
void func_actor_350700_801635A8(Task* arg0);
s32  func_actor_350700_801636A8(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3);

/// Spawn, tick and exit handlers of the enemy actor, dispatched by
/// `func_actor_350700_80162398`.
const TaskFuncTable3 D_actor_350700_80161E24 = { {
    func_actor_350700_80162404,
    func_actor_350700_80161E88,
    func_actor_350700_80162494,
} };

/// Tick handlers of the enemy actor, indexed by `Actor350500Work::field_4C2`:
/// turn to face `target`, start moving, approach until arrival, then turn to
/// the placement yaw.
const TaskFuncTable4 D_actor_350700_80161E30 = { {
    func_actor_350700_80162540,
    func_actor_350700_8016261C,
    func_actor_350700_80162070,
    func_actor_350700_80162764,
} };

/// The constant local-space offset `func_actor_350700_8016261C` rotates:
/// straight ahead along the part's own +Z.
const VECTOR D_actor_350700_80161E40 = { 0, 0, 0x200000, 0 };

/// Spawn, tick and exit handlers of the child part tasks, dispatched by
/// `func_actor_350700_80163274`.
const TaskFuncTable3 D_actor_350700_80161E50 = { {
    func_actor_350700_801632CC,
    func_actor_350700_80163348,
    taskKill,
} };

/// Spawn, tick and exit handlers of the parent actor, dispatched by
/// `func_actor_350700_80163350`.
const TaskFuncTable3 D_actor_350700_80161E5C = { {
    func_actor_350700_80162B30,
    func_actor_350700_80162D5C,
    func_actor_350700_801633BC,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `Actor350700MainWork::field_4FA`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
const TaskFuncTable4 D_actor_350700_80161E68 = { {
    func_actor_350700_80163468,
    func_actor_350700_80163528,
    func_actor_350700_80162F7C,
    func_actor_350700_801635A8,
} };

/// The parent's copy of the forward offset, rotated by
/// `func_actor_350700_80163528`.
const VECTOR D_actor_350700_80161E78 = { 0, 0, 0x200000, 0 };

/// Per-frame tick of the enemy actor: dispatches through the local two-entry table
/// the counter at `field_4C0` indexes, then integrates the local-space `step`
/// into the 16.16 accumulators at `field_4A0`, adds their high halves to the
/// root coordinate's translation and truncates them back to 16 bits. Ticks the
/// animation slots while `field_43C` is set, and -- unless the display object's
/// `flags` carry 0x80 -- draws the ground-shadow quad from the second
/// part's world matrix, clears that part's `flg` and rebuilds its coordinate.
/// The `field_4C5` countdown then runs while it is non-negative, freeing the
/// model buffers on the frame it reaches zero; the init's -1 disables it.
void func_actor_350700_80161E88(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350700_801624D0, func_actor_350700_801624D8 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    funcs[(s16)work->field_4C0](arg0);
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->field_4A0   += work->step.vx;
    work->field_4A4   += work->step.vy;
    work->field_4A8   += work->step.vz;
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
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_4C5 >= 0) {
        if (work->field_4C5 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C5--;
    }
}

/// State handler at index 2 of `D_actor_350700_80161E30`, the approach test.
/// Once the X/Z distances from the root coordinate to `target` stop shrinking
/// below `limit`, plays anim 0x7D3 with a preset carrying the `field_43F` byte,
/// clears `step` and advances `field_4C2`; otherwise records the distances as
/// the new `limit`.
void func_actor_350700_80162070(Task* arg0)
{
    Actor350500Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor350700AnimPreset preset;

    work  = (Actor350500Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_350700_80162860(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4C2++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Spawn-placement message handler: seeds the work block's position and
/// rotation from `place`, picks the start animation from `anim` (or anim 3,
/// 2 once `field_4C4` is set) and installs it with the body of
/// `func_actor_350700_80162860` written out inline. Returns 0.
s32 func_actor_350700_801621B4(Task* task, s32 arg1, Actor350700Placement* place, Actor350700SpawnAnim* anim)
{
    Actor350500Work*       work;
    Actor350500Work*       w;
    Actor350700AnimPreset  preset;
    Actor350700AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor350500Work*)task->work;
    w->field_4C0   = 1;
    w->field_4C2   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4B8   = place->rot.vx;
    w->field_4BA   = place->rot.vy;
    w->field_4BC   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_43F   = anim->field_4;
    } else {
        if (w->field_4C4 != 0) {
            preset.field_4 = 2;
        } else {
            preset.field_4 = 3;
        }
        w->field_43F = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor350500Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350700_80169D0C[work->field_43E], ext, work->poses,
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

/// Per-frame dispatcher of the enemy actor: runs its spawn, tick or exit state
/// from `D_actor_350700_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350700_80162398(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit handler. An allocation failure ends the task instead of
/// leaving a half-built actor behind.
void func_actor_350700_80162404(Task* arg0)
{
    Actor350500Work* work;

    work = memCalloc(sizeof(Actor350500Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C5 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_350700_801624B4(arg0);

    arg0->msgTable     = D_actor_350700_80169D1C;
    arg0->exitCallback = func_actor_350700_80162494;
    arg0->state       += 1;
}

/// Exit callback `func_actor_350700_80162404` installs; tears the task down.
void func_actor_350700_80162494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the enemy work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the actor draws with its own
/// lighting.
void func_actor_350700_801624B4(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra;
    work          = (Actor350500Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Tick handler 0 of the enemy actor, selected by `field_4C0`: idle.
void func_actor_350700_801624D0(Task* arg0)
{
}

/// Tick handler 1 of the enemy actor: runs the state handler of
/// `D_actor_350700_80161E30` that `field_4C2` selects.
void func_actor_350700_801624D8(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350700_80161E30;
    sp.funcs[(s16)work->field_4C2](arg0);
}

/// State handler at index 0 of `D_actor_350700_80161E30`: turns the root part
/// toward `work->target`, taking the yaw of the normalised offset from the
/// part's own translation with `ratan2` -- turned half a revolution away while
/// `field_4C4` is clear -- and rebuilding the local matrix from that yaw alone.
/// Clearing `flg` makes the coordinate tree recompute the world matrix, and
/// bumping `field_4C2` moves on to the next handler.
void func_actor_350700_80162540(Task* task)
{
    Actor350500Work*  work;
    Actor350700Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor350500Work*)task->work;
    coord = (Actor350700Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;
    if (work->field_4C4 == 0) {
        rot.vy += 0x7FF;
    }

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4C2++;
}

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors the parent's `func_actor_350700_80163528`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `field_4C2` so the dispatcher runs the next
/// handler. Where the parent's step rotates its offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
void func_actor_350700_8016261C(Task* arg0)
{
    Actor350500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = D_actor_350700_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

/// State handler at index 3 of `D_actor_350700_80161E30`, the turn-to-face body
/// that follows `func_actor_350700_80162540`. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the target `work->field_4BA` is at least
/// 0x61 it steps `vec.vy` toward it by 0x60 -- the step is taken on an `s32`
/// widening of the extracted yaw -- and otherwise snaps the yaw to the target
/// and plays anim 0x7D3, clearing the two body counters. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, which
/// `_gpUpdateCoordTree` picks up once `flg` is cleared.
void func_actor_350700_80162764(Task* arg0)
{
    Actor350500Work*      work;
    ActorMatWords*        words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350500Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = 1;
        preset.field_8  = 1;
        preset.field_C  = 4;
        preset.field_10 = 0;
        func_actor_350700_80162860(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (ActorMatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler of the enemy actor, also called directly by the
/// approach and turn-to-face handlers with a preset of their own. A changed
/// bank index re-seeds the whole animation slot array through `func_800B3F84`
/// from the bank table and forgets the current animation id. A changed
/// animation id is then stored and installed on every slot - through
/// `func_800B4114` when the preset's `field_8` is set and the slots have
/// already been started, through `Gp_AnimResetSlot` otherwise - after which
/// every slot is ticked once and `field_43C` latches. An unchanged id skips all
/// of that. Returns 0.
s32 func_actor_350700_80162860(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3)
{
    Actor350500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor350500Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350700_80169D0C[work->field_43E], ext, work->poses, work->slots);
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

/// Message-0x7D4 handler of the enemy actor: places the root part at
/// `args`. The translation goes straight into the local matrix, the Euler
/// angles into the coordinate's `rot` slot, from which `RotMatrix` rebuilds the
/// rotation; clearing `flg` makes the world matrix be recomputed. Returns 0.
s32 func_actor_350700_80162998(Task* task, s32 msgId, Actor350700Placement* args)
{
    Actor350700Coord* coord;

    coord             = (Actor350700Coord*)((TmdObject*)task->extra)->coords;
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

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 sets the 0x80 flag, under which the tick skips the shadow and the
/// part update, and clears the 4 flag; 1 clears 0x80, allocates the model
/// buffers and clears 4; 2 sets 0x80 and 4 and latches the mode into the
/// `field_4C5` countdown, which frees the buffers when it runs out; 3 clears
/// 0x80 and sets 4. Anything else returns 1 and leaves the object alone; the
/// handled modes return 0.
s32 func_actor_350700_80162A14(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
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
            obj->flags                               |= 0x80;
            ((Actor350500Work*)task->work)->field_4C5 = mode;
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

/// `Gp_DispatchMsg` handler: latches the variant the message's halfword at
/// 0x2 selects into `field_4C4` -- 1 clears it, 2 sets it, anything else
/// leaves it. Always returns 0.
s32 func_actor_350700_80162AF4(Task* task, s32 arg1, Actor350700Msg* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->field_2) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}

/// The parent's spawn handler. Allocates the 0x50C `Actor350700MainWork` block, seeds it, and spawns the
/// three children `D_actor_350700_801708DC` holds -- table entries 1, 2 and 3 --
/// parking them at `field_4FC` / `field_500` / `field_504`. The first two are
/// models: each has `TmdObject::tpage` / `field_25` loaded with the texture
/// page and CLUT row of the `GpAreaPlace` that entry selects, reached through
/// the area key `&gGameSession->at4.loc.view` and indexed by the model id the child's
/// own `spawnArg2` carries at `GpEnemy::placeKey >> 12`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `func_actor_350700_801633DC`, pointing `msgTable` at the
/// message table and installing `func_actor_350700_801633BC` as its exit
/// callback.
void func_actor_350700_80162B30(Task* arg0)
{
    Actor350700MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor350700MainWork*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_508 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_350700_801708DC, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_4FC = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        key.view        = sessionKey->view;
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
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_500 = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
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
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 3, 8, (s32)arg0);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }
    func_actor_350700_801633DC(arg0);
    arg0->msgTable     = D_actor_350700_8017090C;
    arg0->exitCallback = func_actor_350700_801633BC;
    arg0->state       += 1;
}

/// Per-frame tick of the parent actor: dispatches through the local two-entry
/// table `field_4F8` indexes -- the empty `func_actor_350700_801633F8` or the
/// step dispatcher `func_actor_350700_80163400` -- then integrates the
/// per-frame deltas in `step` into the 16.16 accumulators at `field_4D8..field_4E0`,
/// adds their high halves to the root coordinate's translation, clears `flg`
/// and truncates the accumulators back to 16 bits. Ticks the animation slots
/// while `field_474` is set; and, unless the display object's `flags` carry
/// 0x80, draws the ground-shadow quad from the second part's world matrix.
/// While `gGameSession->viewReady` is set it also clears that part's `flg`,
/// rebuilds its coordinate and rebuilds the actor colour; the colour rebuild
/// runs once more unconditionally. The `field_508` countdown then runs while it
/// is non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
void func_actor_350700_80162D5C(Task* arg0)
{
    TmdObject*           ext      = arg0->extra;
    Actor350700MainWork* work     = (Actor350700MainWork*)arg0->work;
    TaskFunc             funcs[2] = { func_actor_350700_801633F8, func_actor_350700_80163400 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    s32                  i;

    funcs[work->field_4F8](arg0);
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->field_4D8   += work->step.vx;
    work->field_4DC   += work->step.vy;
    work->field_4E0   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
    coord->coord.t[1] += (s16)(work->field_4DC >> 16);
    coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
    coord->flg         = 0;
    work->field_4D8    = (u16)work->field_4D8;
    work->field_4DC    = (u16)work->field_4DC;
    work->field_4E0    = (u16)work->field_4E0;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
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
    func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    if (work->field_508 >= 0) {
        if (work->field_508 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_508--;
    }
}

/// Step 2 of the parent: the arrival check. Once the X/Z distances from the
/// root coordinate to `target` stop shrinking below `limit`, plays anim 0x7D3
/// with a preset carrying the `field_477` byte, clears `step` and advances
/// `field_4FA`; otherwise records the distances as the new `limit`.
void func_actor_350700_80162F7C(Task* arg0)
{
    Actor350700MainWork*  work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor350700AnimPreset preset;

    work  = (Actor350700MainWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_350700_801636A8(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4FA++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Message-0x7DD handler of the parent: starts the motion sequence, storing
/// the placement position as `target` and its rotation in
/// `field_4F0..field_4F4`, then applies a start preset -- `anim`'s, or anim 0xD
/// with preset byte 1 when absent -- with the body of
/// `func_actor_350700_801636A8` written out inline. Returns 0.
s32 func_actor_350700_801630C0(Task* task, s32 arg1, Actor350700Placement* place, Actor350700SpawnAnim* anim)
{
    Actor350700MainWork*   work;
    Actor350700MainWork*   w;
    Actor350700AnimPreset  preset;
    Actor350700AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor350700MainWork*)task->work;
    w->field_4F8   = 1;
    w->field_4FA   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4F0   = place->rot.vx;
    w->field_4F2   = place->rot.vy;
    w->field_4F4   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_477   = anim->field_4;
    } else {
        preset.field_4 = 0xD;
        w->field_477   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor350700MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_350700_801708D8[work->field_476], ext, work->poses,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
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

/// State dispatcher of the child part tasks: copies the three-handler table
/// `D_actor_350700_80161E50` onto the stack and runs the entry `Task::state`
/// selects.
void func_actor_350700_80163274(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E50;
    sp.funcs[task->state](task);
}

/// Spawn state of a child part task: chains its root coordinate under the
/// parent's part coordinate the spawn arguments name, inherits the parent's
/// light/colour matrices, reparents the task so it is updated with the parent,
/// and advances to the next state.
void func_actor_350700_801632CC(Task* task)
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

/// Tick state of the child part tasks: nothing to do, the parent drives them.
void func_actor_350700_80163348(Task* task)
{
}

/// Per-frame dispatcher of the parent actor: runs its spawn, tick or exit
/// state from `D_actor_350700_80161E5C`, skipping the frame while the global
/// freeze byte is set.
void func_actor_350700_80163350(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E5C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback `func_actor_350700_80162B30` installs, the same
/// `Gp_EnemyTaskExit` teardown `func_actor_350700_80162494` performs.
void func_actor_350700_801633BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the parent work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the parent draws with its own
/// lighting.
void func_actor_350700_801633DC(Task* task)
{
    TmdObject*           ext;
    Actor350700MainWork* work;

    ext           = task->extra;
    work          = (Actor350700MainWork*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// The empty first entry of the parent's two-handler table, selected by
/// `Actor350700MainWork::field_4F8` -- the idle half of the pair whose other
/// entry is the step dispatcher `func_actor_350700_80163400`.
void func_actor_350700_801633F8(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table
/// `D_actor_350700_80161E68` onto the stack and runs the entry `field_4FA`
/// selects.
void func_actor_350700_80163400(Task* task)
{
    Actor350700MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor350700MainWork*)task->work;
    fns  = D_actor_350700_80161E68;
    fns.funcs[(s16)work->field_4FA](task);
}

/// Step 0 of the parent: turns the root part to face `work->target`, taking
/// the yaw of the normalised offset from the part's own translation with
/// `ratan2` and rebuilding the local matrix from that yaw alone, then advances
/// the step.
void func_actor_350700_80163468(Task* task)
{
    Actor350700MainWork* work;
    Actor350700Coord*    coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor350700MainWork*)task->work;
    coord = (Actor350700Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4FA++;
}

/// Step 1 of the parent: rotates the constant forward offset
/// `D_actor_350700_80161E78` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it, and
/// advances the step.
void func_actor_350700_80163528(Task* task)
{
    Actor350700MainWork* work;
    GsCOORDINATE2*       coord;
    VECTOR               vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor350700MainWork*)task->work;

    vec = D_actor_350700_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}

/// Step 3 of the parent: turn to the placement yaw. Euler-extracts the root
/// coordinate into `vec`, and while the yaw gap to `work->field_4F2` is at
/// least 0x41 it steps `vec.vy` toward it by 0x40, taking the step on an `s32`
/// widening of the extracted yaw; otherwise it snaps the yaw to the target and
/// plays anim 0x7D3 with a preset carrying the `field_477` byte, clearing the
/// two motion counters. Either way the root coordinate is rebuilt as the
/// identity matrix rotated by `vec`.
void func_actor_350700_801635A8(Task* arg0)
{
    Actor350700MainWork*  work;
    ActorMatWords*        words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350700MainWork*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F2 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4F2;
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_350700_801636A8(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
    }

    words          = (ActorMatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler of the parent, also called directly by the arrival
/// and turn steps: re-seeds the 20-slot array off bank table
/// `D_actor_350700_801708D8` when the preset's bank index changes, then
/// stores the animation id and restarts or resets every slot with it and ticks
/// them, latching `field_474`. Unlike the enemy actor's handler it reinstalls
/// the animation even when the id is unchanged. Returns 0.
s32 func_actor_350700_801636A8(Task* task, s32 arg1, Actor350700AnimPreset* msg, s32 arg3)
{
    Actor350700MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor350700MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_350700_801708D8[work->field_476], ext, work->poses,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
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

/// Message-0x7D4 handler of the parent: places the root part at `args`, the
/// translation straight into the local matrix and the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation.
/// Returns 0.
s32 func_actor_350700_801637C4(Task* task, s32 msgId, Actor350700Placement* args)
{
    Actor350700Coord* coord;

    coord             = (Actor350700Coord*)((TmdObject*)task->extra)->coords;
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

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`,
/// then the resulting flags are republished onto the objects of the three
/// child tasks the spawn handler parked at `field_4FC` / `field_500` /
/// `field_504`. The modes are those of `func_actor_350700_80162A14`, the
/// countdown mode 2 latches being `field_508`. Anything else returns 1 and
/// leaves the object alone; the handled modes return 0.
s32 func_actor_350700_80163840(Task* task, s32 arg1, s32 mode)
{
    Actor350700MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    TmdObject*           objC;
    u16                  flags;
    s32                  ret;

    work = (Actor350700MainWork*)task->work;
    obj  = task->extra;
    objA = work->field_4FC->extra;
    objB = work->field_500->extra;
    objC = work->field_504->extra;
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
            work->field_508 = mode;
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
    flags       = obj->flags;
    objB->flags = flags;
    objA->flags = flags;
    objC->flags = flags;
    return ret;
}

/// Message handler that accepts its message and does nothing with it:
/// returns 0.
s32 func_actor_350700_8016395C(void)
{
    return 0;
}
