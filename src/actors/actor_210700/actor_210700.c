#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's work block. The spawn handler `func_actor_210700_80149F90`
/// allocates it zeroed with `memCalloc(0x540, 0)` and keeps it in
/// `Task::work`. The front is the animation state the 0x7D3 message handler
/// drives - the context, its slots and the pose buffer handed to
/// `func_800B3F84` - followed by the light / colour matrices
/// `func_actor_210700_8014A208` points the model at, and the texture-upload
/// state the upload handler runs.
typedef struct Actor210700Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /// Non-zero once an animation has been started; gates the per-frame tick
    /// of slots 1..0x13.
    /* 0x474 */ s32 field_474;
    /// Animation id the slots were last reset to; -1 out of the spawn handler
    /// and whenever a new animation source is loaded.
    /* 0x478 */ s32 field_478;
    /// Index of the animation source last loaded from
    /// `D_actor_210700_801585C8`; -1 out of the spawn handler.
    /* 0x47C */ s32    field_47C;
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ byte   pad_4C0[0x78];
    /// Countdown reload value for the texture upload; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x538 */ s16 field_538;
    /// Frames left before the next texture-upload step.
    /* 0x53A */ u16 field_53A;
    /// Texture-upload step in progress, 0 when idle; set to 1 by the 0x7E0
    /// handler's mode 3.
    /* 0x53C */ s16 field_53C;
    /// Frames until the model buffers are freed; -1 when idle, latched to 2 by
    /// the 0x7D5 handler's mode 2.
    /* 0x53E */ s16 field_53E;
} Actor210700Work;
STATIC_ASSERT_SIZEOF(Actor210700Work, 0x540);

/// Payload of the 0x7D3 animation message. `field_0` indexes the animation
/// source table `D_actor_210700_801585C8`, `field_4` is the animation id every
/// slot 1..0x13 is reset to, and a non-zero `field_8` resets the slots through
/// `func_800B4114` instead of `Gp_AnimResetSlot`. Only the first three words
/// are read; the spawn handler's frame spaces its locals as if the block were
/// 0x18 bytes.
typedef struct _Actor210700Anim {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[0xC];
} Actor210700Anim;
STATIC_ASSERT_SIZEOF(Actor210700Anim, 0x18);

/// Animation sources the 0x7D3 handler loads, indexed by its payload's
/// `field_0`.
extern void* D_actor_210700_801585C8[];

/// The actor's message table, parked in `Task::msgTable`: 0x7D3
/// `func_actor_210700_8014A224`, 0x7D4 `func_actor_210700_8014A344`, 0x7D5
/// `func_actor_210700_8014A3D4`, 0x7E0 `func_actor_210700_8014A4B0`.
extern GpMsgEntry D_actor_210700_801585D8[];

/// Images the texture-upload state and the 0x7E0 handler post over the
/// model's texture.
extern GpImgRec D_actor_210700_80157F4C;
extern GpImgRec D_actor_210700_8015826C;
extern GpImgRec D_actor_210700_8015858C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_210700_80149F90(Task* task);
void func_actor_210700_8014A0AC(Task* task);
void func_actor_210700_8014A1E8(Task* task);
void func_actor_210700_8014A208(Task* arg0);
s32  func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3);
s32  func_actor_210700_8014A344(Task* task, s32 arg1, GpXformArg* args, s32 arg3);

/// The actor's three task states - spawn, tick and teardown - which
/// `func_actor_210700_80149F38` runs by `Task::state`.
const TaskFuncTable3 D_actor_210700_80149E24 = { {
    func_actor_210700_80149F90,
    func_actor_210700_8014A0AC,
    func_actor_210700_8014A1E8,
} };

/// Texture-upload step, run by the tick state: while `field_53C` names an
/// upload in progress, counts `field_53A` down one a frame, and on the frame
/// it underflows posts that step's image over the 0x18x0x10 rect at y 0x28.
/// Steps 1 and 2 then reload the countdown from `field_538` and advance to the
/// next step; step 3 returns to idle.
void func_actor_210700_80149E30(Task* arg0)
{
    Actor210700Work* work;
    RECT             rect;

    work   = (Actor210700Work*)((GameActor*)arg0->work);
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_53C) {
        case 1:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015858C, &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 2:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015826C, &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 3:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_80157F4C, &rect);
                work->field_53C = 0;
            }
            break;
    }
}

/// The actor's task entry: runs the handler for the task's current state out
/// of `D_actor_210700_80149E24` - spawn, per-frame tick or teardown - copying
/// the table onto the stack before the call.
void func_actor_210700_80149F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_210700_80149E24;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the zeroed work block into `Task::work` (handing
/// the task to `Gp_EnemyTaskExit` if that fails), marks no animation loaded,
/// hides the model with `TmdObject::flags` bit 0x80, then runs its own 0x7D4
/// and 0x7D3 message handlers directly to place the actor at the origin -
/// which shows it again - and start animation 1 of source 0. It draws the
/// ground shadow under the model's second part, points the model at the work
/// block's light / colour matrices, installs the message table and the exit
/// callback, and advances to the tick state.
void func_actor_210700_80149F90(Task* task)
{
    Actor210700Work* work;
    TmdObject*       extra;
    GpXformArg       args;
    Actor210700Anim  anim;
    VECTOR3          pos;

    extra = (TmdObject*)task->extra;
    work  = (Actor210700Work*)memCalloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_53E = -1;
    extra->flags    = 0x80;
    args.pos.vx     = 0;
    args.pos.vy     = 0;
    args.pos.vz     = 0;
    args.rot.vx     = 0;
    args.rot.vy     = 0;
    args.rot.vz     = 0;
    func_actor_210700_8014A344(task, 0x7D4, &args, 0);
    anim.field_0 = 0;
    anim.field_4 = 1;
    anim.field_8 = 0;
    func_actor_210700_8014A224(task, 0x7D3, &anim, 0);
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->groundShade);
    }
    func_actor_210700_8014A208(task);
    task->msgTable     = D_actor_210700_801585D8;
    task->exitCallback = func_actor_210700_8014A1E8;
    task->state++;
}

/// Tick state: while an animation is running ticks slots 1..0x13 and draws
/// the ground shadow under the model's second part. While the game session's
/// view is ready it invalidates and rebuilds that part's coordinate and hands
/// it to `func_800D7A9C`. It then runs the texture-upload step and counts
/// `field_53E` down, freeing the model buffers on the frame it reaches 0.
void func_actor_210700_8014A0AC(Task* task)
{
    Actor210700Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s16              count;
    s32              i;

    work = (Actor210700Work*)task->work;
    ext  = task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->groundShade);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    func_actor_210700_80149E30(task);
    count = work->field_53E;
    if (count >= 0) {
        if (count == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_53E = (s16)((u16)work->field_53E - 1);
    }
}

/// The actor's teardown state and `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
void func_actor_210700_8014A1E8(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the model at the work block's light and colour matrices, so the
/// actor is lit from its own block rather than the defaults.
void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*       ext;
    Actor210700Work* work;

    work          = (Actor210700Work*)arg0->work;
    ext           = arg0->extra;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Message-0x7D3 handler: starts an animation. A source index different from
/// the one last loaded seeds the animation context from that entry of
/// `D_actor_210700_801585C8` and forgets the current animation id; a new
/// animation id then resets slots 1..0x13 to it - through `func_800B4114`
/// when the payload's `field_8` is set, `Gp_AnimResetSlot` otherwise - ticks
/// them once and enables the per-frame tick. Always returns 0.
s32 func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* msg, s32 arg3)
{
    Actor210700Work* work;
    s32              i;
    register s32     id asm("v1");
    TmdObject*       ext;

    work = (Actor210700Work*)task->work;
    id   = msg->field_0;
    ext  = (TmdObject*)task->extra;
    if (id != work->field_47C) {
        work->field_478 = -1;
        work->field_47C = id;
        func_800B3F84(&work->rig.anim, D_actor_210700_801585C8[id], ext, work->rig.poses, work->rig.slots);
    }
    if (msg->field_4 != work->field_478) {
        work->field_478 = msg->field_4;
        if (msg->field_8 != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->rig.anim, i, work->field_478, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->field_478);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->field_474 = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the actor. Writes the payload's translation
/// into the root coordinate's local matrix and its Euler angles into the
/// coordinate's `rot` slot, rebuilds the rotation from them, clears `flg` so
/// the world matrix is recomputed, and clears `TmdObject::flags` bit 0x80 to
/// show the model. Always returns 0.
s32 func_actor_210700_8014A344(Task* task, s32 arg1, GpXformArg* args, s32 arg3)
{
    GpCoordExt* coord;
    TmdObject*  extra;

    extra               = (TmdObject*)task->extra;
    coord               = (GpCoordExt*)extra->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg    = 0;
    extra->flags &= 0xFF7F;
    return 0;
}

/// Message-0x7D5 handler: sets the model's visibility and mode bit from the
/// message's mode word. `TmdObject::flags` bit 0x80 hides the model and bit
/// 0x4 is the one modes 2 and 3 raise. Mode 0 hides the model and drops 0x4,
/// 1 shows it, reallocates its buffers through `Tmd_AllocBuffers` and drops
/// 0x4, 2 hides it, raises 0x4 and starts the work block's `field_53E`
/// countdown to freeing the buffers, and 3 shows it and raises 0x4. Handled
/// modes return 0; anything else returns 1 and changes nothing.
s32 func_actor_210700_8014A3D4(Task* arg0, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor210700Work* work;
    s32              ret;

    obj  = arg0->extra;
    work = (Actor210700Work*)((GameActor*)arg0->work);
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
            work->field_53E = mode;
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

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x18x0x10 rect at y 0x28 -- `D_actor_210700_8015858C` for mode 1,
/// `D_actor_210700_80157F4C` for modes 0 and 2, and `D_actor_210700_8015826C`
/// for mode 3, which sets the work block's `field_53C` / `field_538` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_210700_8014A4B0(Task* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_210700_8015858C;
            break;
        case 0:
        case 2:
            img = &D_actor_210700_80157F4C;
            break;
        case 3:
            ((Actor210700Work*)((GameActor*)arg0->work))->field_53C = 1;
            ((Actor210700Work*)((GameActor*)arg0->work))->field_538 = 1;
            img                                                     = &D_actor_210700_8015826C;
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}
