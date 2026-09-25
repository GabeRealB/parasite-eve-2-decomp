#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "actors/actor_350500.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x14-byte animation preset the message-0x7D3 handler
/// `func_actor_350500_80162828` takes: `field_0` is the bank index,
/// `field_4` the animation id, `field_8` selects a blended restart once the
/// slots run and `field_C` is handed to that restart. The placement, approach
/// and final-turn steps build one on their own stack.
typedef struct Actor350500AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor350500AnimPreset;
STATIC_ASSERT_SIZEOF(Actor350500AnimPreset, 0x14);

/// Position and rotation a placement message carries.
typedef struct Actor350500Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor350500Placement;
STATIC_ASSERT_SIZEOF(Actor350500Placement, 0x18);

/// Optional start animation the placement handler takes: the preset's
/// `field_4` and the `field_43F` byte. Absent, the defaults are anim 3 (or 2
/// once `field_4C4` is set) and 1.
typedef struct Actor350500SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor350500SpawnAnim;

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_350500_8016272C` writes over the root coordinate before
/// `RotMatrix` overwrites the 3x3: five aligned stores rather than nine
/// halfword ones.
typedef struct Actor350500MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor350500MatWords;
STATIC_ASSERT_SIZEOF(Actor350500MatWords, 0x14);

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`) holds the Euler angles the placement
/// and face-the-target steps write and hand straight to `RotMatrix`.
typedef struct Actor350500Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor350500Coord;
STATIC_ASSERT_SIZEOF(Actor350500Coord, 0x4C);

/// Ground-shadow quad the tick feeds the second part's world translation to.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Global freeze byte in the main executable; the state dispatcher runs
/// nothing while it is non-zero.
extern u8 D_801153F4;

/// Animation bank table the preset's bank index selects from.
extern void* D_actor_350500_80168EA0[];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350500_801623CC`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350500_80168EB0[];

void func_actor_350500_80161E50(Task* arg0);
void func_actor_350500_80162038(Task* arg0);
void func_actor_350500_801623CC(Task* arg0);
void func_actor_350500_8016245C(Task* arg0);
void func_actor_350500_8016247C(Task* arg0);
void func_actor_350500_80162498(Task* arg0);
void func_actor_350500_801624A0(Task* arg0);
void func_actor_350500_80162508(Task* task);
void func_actor_350500_801625E4(Task* arg0);
void func_actor_350500_8016272C(Task* arg0);
s32  func_actor_350500_80162828(Task* task, s32 arg1, Actor350500AnimPreset* msg, s32 arg3);

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
const TaskFuncTable3 D_actor_350500_80161E24 = { {
    func_actor_350500_801623CC,
    func_actor_350500_80161E50,
    func_actor_350500_8016245C,
} };

/// The walk steps, indexed by `Actor350500Work::field_4C2`: turn to face
/// `target`, start moving, approach until arrival, then turn to the placement
/// yaw.
const TaskFuncTable4 D_actor_350500_80161E30 = { {
    func_actor_350500_80162508,
    func_actor_350500_801625E4,
    func_actor_350500_80162038,
    func_actor_350500_8016272C,
} };

/// Local-space offset the start-moving step rotates: straight ahead along
/// the part's own +Z.
const VECTOR D_actor_350500_80161E40 = { 0, 0, 0x200000, 0 };

/// Per-frame tick: runs the idle or the walk handler `field_4C0` selects,
/// then integrates the world-space `step` into the 16.16 accumulators at
/// `field_4A0`, adds their high halves to the root coordinate's translation
/// and truncates them back to 16 bits. Ticks the animation slots while
/// `field_43C` is set, and -- unless the model is hidden -- draws the
/// ground-shadow quad under the second part, clears that part's `flg` and
/// rebuilds its coordinate. The `field_4C5` countdown then runs while it is
/// non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
void func_actor_350500_80161E50(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350500_80162498, func_actor_350500_801624A0 };
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

/// Walk step 2, the approach test. Once the X/Z distances from the root
/// coordinate to `target` stop shrinking below `limit`, plays the `field_43F`
/// animation, clears `step` and advances `field_4C2`; otherwise records the
/// distances as the new `limit`.
void func_actor_350500_80162038(Task* arg0)
{
    Actor350500Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor350500AnimPreset preset;

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
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4C2++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Placement message handler: seeds the work block's target and placement
/// rotation from `place`, starts the walk sequence, and picks the start
/// animation from `anim` (or anim 3, 2 once `field_4C4` is set), installing
/// it with the body of `func_actor_350500_80162828` written out inline.
/// Returns 0.
s32 func_actor_350500_8016217C(Task* task, s32 arg1, Actor350500Placement* place, Actor350500SpawnAnim* anim)
{
    Actor350500Work*       work;
    Actor350500Work*       w;
    Actor350500AnimPreset  preset;
    Actor350500AnimPreset* msg;
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
        func_800B3F84(&work->anim, D_actor_350500_80168EA0[work->field_43E], ext, work->poses,
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

/// Per-frame dispatcher: runs the spawn, tick or exit state from
/// `D_actor_350500_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350500_80162360(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350500_80161E24;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and the three cleared words the work's own init expects, republishes the
/// light and colour matrices onto the display object, then installs the
/// message table and the exit handler. An allocation failure ends the
/// task instead of leaving a half-built actor behind.
void func_actor_350500_801623CC(Task* arg0)
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

    func_actor_350500_8016247C(arg0);

    arg0->msgTable     = D_actor_350500_80168EB0;
    arg0->exitCallback = func_actor_350500_8016245C;
    arg0->state        = arg0->state + 1;
}

/// Exit callback `func_actor_350500_801623CC` installs; tears the task down.
void func_actor_350500_8016245C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the work block's two matrices onto `TmdObject::lightMtx` /
/// `colorMtx`, so the actor draws with its own lighting.
void func_actor_350500_8016247C(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra;
    work          = (Actor350500Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Idle tick handler, selected while `field_4C0` is clear.
void func_actor_350500_80162498(Task* arg0)
{
}

/// Walk tick handler: runs the step of `D_actor_350500_80161E30` that
/// `field_4C2` selects.
void func_actor_350500_801624A0(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350500_80161E30;
    sp.funcs[(s16)work->field_4C2](arg0);
}

/// Walk step 0: turns the root part toward `work->target`, taking the yaw of
/// the normalised offset from the part's own translation with `ratan2` --
/// turned half a revolution away while `field_4C4` is clear -- and rebuilding
/// the local matrix from that yaw alone. Clearing `flg` makes the coordinate
/// tree recompute the world matrix, and bumping `field_4C2` moves on to the
/// next step.
void func_actor_350500_80162508(Task* task)
{
    Actor350500Work*  work;
    Actor350500Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor350500Work*)task->work;
    coord = (Actor350500Coord*)((TmdObject*)task->extra)->coords;

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

/// Walk step 1: rotates `D_actor_350500_80161E40` -- shrunk to -0.4 of its
/// length, a slower backward walk, while `field_4C4` is clear -- through the
/// root part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the approach test, and advances `field_4C2`.
void func_actor_350500_801625E4(Task* arg0)
{
    Actor350500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = D_actor_350500_80161E40;
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

/// Walk step 3, the final turn. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the placement yaw `field_4BA` is at least
/// 0x61 steps `vec.vy` toward it by 0x60; otherwise snaps the yaw to it,
/// plays anim 1 and clears `field_4C0` and `field_4C2`, which returns the
/// tick to idle. Either way the root coordinate is rebuilt as the identity
/// rotated by `vec` and its `flg` cleared.
void func_actor_350500_8016272C(Task* arg0)
{
    Actor350500Work*      work;
    Actor350500MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350500AnimPreset preset;
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
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor350500MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler, also called directly by the approach and final-turn
/// steps with a preset of their own. A changed bank index re-seeds the
/// animation slot array through `func_800B3F84` from the bank table and
/// forgets the current animation id. A changed animation id is then stored and
/// installed on every slot - through `func_800B4114` when the preset's
/// `field_8` is set and the slots have already been started, through
/// `Gp_AnimResetSlot` otherwise - after which every slot is ticked once and
/// `field_43C` latches. An unchanged id skips all of that. Returns 0.
s32 func_actor_350500_80162828(Task* task, s32 arg1, Actor350500AnimPreset* msg, s32 arg3)
{
    Actor350500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor350500Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350500_80168EA0[work->field_43E], ext, work->poses, work->slots);
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

/// Message-0x7D4 handler: places the root part at `args`. The translation
/// goes straight into the local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation;
/// clearing `flg` makes the world matrix be recomputed. Returns 0.
s32 func_actor_350500_80162960(Task* task, s32 msgId, Actor350500Placement* args)
{
    Actor350500Coord* coord;

    coord             = (Actor350500Coord*)((TmdObject*)task->extra)->coords;
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
/// Mode 0 hides the model (flag 0x80, under which the tick also skips the
/// shadow and the part update) and clears the 4 flag; 1 shows it, allocates
/// the model buffers and clears 4; 2 hides it, sets 4 and latches the mode
/// into the `field_4C5` countdown, which frees the buffers when it runs out;
/// 3 shows it and sets 4. Anything else returns 1 and leaves the object
/// alone; the handled modes return 0.
s32 func_actor_350500_801629DC(Task* task, s32 arg1, s32 mode)
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
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, Actor350500Msg* msg)
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
