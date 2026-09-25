#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actors_shared_8013231c.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x14-byte animation preset `func_actor_113000_80132208` reads: `field_0` is
/// the animation bank index it compares against `Actor113000Work::field_47C`,
/// `field_4` the animation id it stores in `field_478`, and `field_8` selects
/// between `func_800B4114` and `Gp_AnimResetSlot`.
typedef struct Actor113000AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor113000AnimPreset;
STATIC_ASSERT_SIZEOF(Actor113000AnimPreset, 0x14);

/// Animation source table `func_actor_113000_80132208` indexes by the preset's
/// bank index and hands `func_800B3F84` as its data argument.
extern void* D_actor_113000_8013ABB0[];

/// Work block this actor allocates in its spawn handler and parks in
/// `Task::work`. It is fronted by a `GpAnimCtx`: the start-preset handler
/// passes the block itself, its `slots` array and the pose buffer after them
/// to `func_800B3F84`, and the per-frame tick walks slots 1..0x13. `light` /
/// `color` are the matrices the TMD object's `lightMtx` / `colorMtx` are
/// pointed at.
typedef struct Actor113000Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
    /// The pose buffer `func_800B3F84` is handed, immediately after the
    /// slots.
    /* 0x334 */ byte field_334[0x140];
    /// Raised once a preset has started the slots; the per-frame tick only
    /// advances them while it is set.
    /* 0x474 */ s32    field_474;
    /* 0x478 */ s32    field_478; ///< -1 out of the spawn handler
    /* 0x47C */ s32    field_47C; ///< -1 out of the spawn handler
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ s16    field_4C0; ///< upload countdown reload; set to 1 alongside `field_4C4` by mode 3
    /* 0x4C2 */ u16    field_4C2; ///< upload countdown the per-frame state runs down, reloaded from `field_4C0` on underflow
    /* 0x4C4 */ s16    field_4C4; ///< upload step 1..3; set to 1 alongside `field_4C0` by mode 3
    /* 0x4C6 */ s16    field_4C6; ///< cleared by the spawn handler
    /* 0x4C8 */ s16    field_4C8; ///< countdown to `Tmd_FreeBuffers`, idle below 0; -1 out of the spawn handler, 2 from display mode 2
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor113000Work;
STATIC_ASSERT_SIZEOF(Actor113000Work, 0x4CC);

/// The actor's three texture records, one per mode of the message-0x7E0
/// handler. Each is a lone `GpImgRec` whose 0x20x0x10 source rect repeats the
/// size the upload code's scratch `RECT` carries and whose `data` points at
/// its pixel blob; the three sit 0x420 bytes apart in the overlay's data
/// segment.
extern GpImgRec D_actor_113000_8013A32C;
extern GpImgRec D_actor_113000_8013A74C;
extern GpImgRec D_actor_113000_8013AB6C;

/// Message dispatch table the spawn handler parks in `Task::msgTable`:
/// message id / handler pairs, terminated by 0x7FFFFFFF and a null word.
extern u8 D_actor_113000_8013ABC0[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_113000_80131F90(Task* task);
void func_actor_113000_80132070(Task* task);
void func_actor_113000_801321A8(Task* task);

/// The actor's three task states, which `func_actor_113000_80131F38` runs by
/// `Task::state`: spawn, per-frame tick and exit.
const TaskFuncTable3 D_actor_113000_80131E24 = { {
    func_actor_113000_80131F90,
    func_actor_113000_80132070,
    Gp_EnemyTaskExit,
} };

/// Texture-upload state: runs the countdown at `field_4C2` down one a frame
/// while `field_4C4` names the upload step in progress, and on the frame it
/// underflows posts that step's image over the 0x20x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4C0` and advancing `field_4C4` for
/// steps 1 and 2, or clearing it for step 3, which ends the sequence until
/// mode 3 of the message-0x7E0 handler restarts it. Step 0 does nothing.
void func_actor_113000_80131E30(Task* arg0)
{
    Actor113000Work* work;
    RECT             rect;

    work   = (Actor113000Work*)((GameActor*)arg0->work);
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (work->field_4C4) {
        case 1:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013AB6C, &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 2:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A74C, &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 3:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A32C, &rect);
                work->field_4C4 = 0;
            }
            break;
    }
}

/// Task callback of the actor: copies the three-handler table
/// `D_actor_113000_80131E24` (spawn `func_actor_113000_80131F90`, per-frame
/// tick `func_actor_113000_80132070`, exit `Gp_EnemyTaskExit`) onto the stack
/// and runs the entry `Task::state` selects.
void func_actor_113000_80131F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113000_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, then hands the model's matrices to the light/color rebuilder.
void func_actor_113000_80131F90(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    u16              flags;

    extra = (TmdObject*)task->extra;
    work  = (Actor113000Work*)memCalloc(0x4CC, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_4C6 = 0;
    work->field_4C8 = -1;
    flags           = extra->flags | 0x80;
    extra->flags    = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
    }
    func_actor_113000_801321A8(task);
    task->msgTable     = D_actor_113000_8013ABC0;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Per-frame tick, run after the model has been published: ticks the animation
/// slots while the preset bank `field_474` marks live, draws the ground shadow
/// under model part 1 while the model is not deferred, rebuilds that part's
/// world matrix while the session's 0x4D is set, runs the texture-upload
/// state, and counts the buffer free at `field_4C8` down to zero.
void func_actor_113000_80132070(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    GsCOORDINATE2*   coords;
    VECTOR3          pos;
    s32              i;

    work  = (Actor113000Work*)task->work;
    extra = (TmdObject*)task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coords        = ((TmdObject*)task->extra)->coords;
        coords[1].flg = 0;
        Gp_UpdateCoord(&coords[1]);
        func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
    }
    func_actor_113000_80131E30(task);
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4C8--;
    }
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_113000_801321A8(Task* task)
{
    Actor113000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor113000Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coords          = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Start-preset handler: a preset bank the work block is not already on
/// re-seeds it -- the animation id is reset to -1, the bank is stored and the
/// bank's animation source goes to `func_800B3F84` with the block's context,
/// its pose buffer and its slots. The preset's
/// animation id is then latched, every slot 1..0x13 restarted -- through
/// `func_800B4114` when the preset asks for it, through `Gp_AnimResetSlot`
/// otherwise -- ticked once, and `field_474` raised.
s32 func_actor_113000_80132208(Task* task, s32 msgId, Actor113000AnimPreset* msg, s32 arg3)
{
    Actor113000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor113000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_47C) {
        work->field_47C = msg->field_0;
        work->field_478 = -1;
        func_800B3F84(&work->anim, D_actor_113000_8013ABB0[work->field_47C], ext, work->field_334,
                      work->slots);
    }
    work->field_478 = msg->field_4;
    if (msg->field_8 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_478, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_478);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}

/// Placement handler: writes the payload's position into the root
/// coordinate's translation and its Euler angles into the coordinate's `rot`
/// slot, rebuilds the rotation from them with `RotMatrix` and clears `flg` so
/// the world matrix is recomputed. Returns 0.
s32 func_actor_113000_8013231C(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
{
    ActorsShared8013231cCoord* coord;

    coord             = (ActorsShared8013231cCoord*)((TmdObject*)task->extra)->coords;
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

/// Display handler: `mode` sets or clears bit 0x80 of `TmdObject::flags`
/// (hidden) and sets or clears bit 0x4:
///
///   mode 0  set 0x80, clear 0x4
///   mode 1  clear 0x80, `Tmd_AllocBuffers`, clear 0x4
///   mode 2  set 0x80, store 2 in the countdown `Actor113000Work::field_4C8`
///           that `func_actor_113000_80132070` ends in `Tmd_FreeBuffers`,
///           set 0x4
///   mode 3  clear 0x80, set 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused.
s32 func_actor_113000_80132398(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    Actor113000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor113000Work*)task->work;
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

/// Message-0x7E0 handler: uploads one of the actor's three texture records over
/// the 0x20x0x10 rect at y 0x28 -- `D_actor_113000_8013AB6C` for mode 1,
/// `D_actor_113000_8013A32C` for modes 0 and 2, and `D_actor_113000_8013A74C`
/// for mode 3, which sets the work block's `field_4C4` / `field_4C0` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_113000_80132474(Task* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_113000_8013AB6C;
            break;
        case 0:
        case 2:
            img = &D_actor_113000_8013A32C;
            break;
        case 3:
            ((Actor113000Work*)((GameActor*)arg0->work))->field_4C4 = 1;
            ((Actor113000Work*)((GameActor*)arg0->work))->field_4C0 = 1;
            img                                                     = &D_actor_113000_8013A74C;
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
