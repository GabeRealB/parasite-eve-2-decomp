#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The enemy's work block, published by its spawn handler and by its task
/// body.
extern Actor151000Work* D_actor_151000_8013D37C;

/// The enemy's task, published by its spawn handler so the visibility opcode
/// can reach its model.
extern Task* D_actor_151000_8013D380;

/// Reset argument the "start animation" opcode leaves behind:
/// `func_actor_151000_801326AC` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 D_actor_151000_8013D2AC;

/// Picks the distance the runner walks the model each frame: 0 steps 0x3C
/// forward, 1 steps 0xF back, 2 steps 0x19 forward. Set by the "walk to"
/// opcode.
extern s16 D_actor_151000_8013D384;

/// Fade countdown: `func_actor_151000_80131EE0` seeds it, and the fade task
/// `func_actor_151000_80131E24` draws while it is non-zero.
extern s32 D_actor_151000_8013D378;

/// Descriptor of the fade task `func_actor_151000_80131E24`.
extern TaskDesc D_actor_151000_80133360;

/// The enemy's message table and the animation data its work block's slots
/// are seeded from.
extern GpMsgEntry D_actor_151000_8013D2B0[];
extern u8         D_actor_151000_8013D2EC[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_151000_80132084(Task* task);
void func_actor_151000_80132450(GpEnemy* enemy, Task* task);
void func_actor_151000_801324D4(Task* task);
void func_actor_151000_801324FC(Task* task);
void func_actor_151000_801325C4(void);
void func_actor_151000_80132610(void);
void func_actor_151000_801326AC(void);
void func_actor_151000_80132A38(Task* task);

/// The fade task: while the countdown `D_actor_151000_8013D378` is non-zero,
/// draws a full-screen black `TILE` into ordering table slot 0xA; once it is
/// zero the task kills itself.
void func_actor_151000_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_151000_8013D378 != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(gGpuCurrentOt + 0xA, tile);
    } else {
        taskKill(task);
    }
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_151000_80131EE0(s32 frames)
{
    D_actor_151000_8013D378 = frames;
    if (frames != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

/// State 0 of the enemy's task: allocates the work block, publishes it in
/// `D_actor_151000_8013D37C` and on the task's work slot, points the model's
/// light and colour matrices and its animation context at it, publishes the
/// task in `D_actor_151000_8013D380`, then runs the runner once and advances
/// the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
void func_actor_151000_80131F1C(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_151000_8013D37C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_151000_801324D4;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_151000_8013D37C->light;
    obj->colorMtx                = &D_actor_151000_8013D37C->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_151000_8013D380      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_151000_8013D37C->rig.anim, D_actor_151000_8013D2EC, obj,
                  &D_actor_151000_8013D37C->rig.poses, D_actor_151000_8013D37C->rig.slots);
    D_actor_151000_8013D37C->st.animId  = 1;
    D_actor_151000_8013D37C->st.state   = 2;
    D_actor_151000_8013D37C->st.travel  = 0;
    D_actor_151000_8013D37C->turnFrames = 0;
    D_actor_151000_8013D37C->stepRec    = 0;
    D_actor_151000_8013D37C->footsteps  = 0;
    task->msgTable                      = D_actor_151000_8013D2B0;
    func_actor_151000_80132084(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation restart and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `D_actor_151000_8013D384`) and, when the
/// walk ends, queues clip 0xD through state 1; it turns the model while
/// `turnFrames` counts down in clip 3, then ticks the animation and, once
/// `footsteps` is set, plays the footsteps.
void func_actor_151000_80132084(Task* task)
{
    GsCOORDINATE2*   coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_151000_8013D37C->st.state == 1) {
        func_actor_151000_801326AC();
        D_actor_151000_8013D37C->st.state = 3;
    } else if (D_actor_151000_8013D37C->st.state == 2) {
        func_actor_151000_80132610();
        D_actor_151000_8013D37C->st.state = 3;
    } else if (D_actor_151000_8013D37C->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_151000_8013D384) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_151000_8013D2AC = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_151000_801325C4();
        if (work->footsteps != 0) {
            func_actor_151000_801324FC(task);
        }
    }
}

/// The enemy's task body: publishes the task's work block in
/// `D_actor_151000_8013D37C`, then runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_151000_80131F1C`,
/// then the per-frame `func_actor_151000_80132450`.
void func_actor_151000_801323F4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_151000_80131F1C,
        func_actor_151000_80132450,
    };

    D_actor_151000_8013D37C = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the enemy's task: refreshes the model root's coordinate, hands
/// `func_800D7A9C` the point 0x320 above it, then runs the runner and draws the
/// ground shadow.
void func_actor_151000_80132450(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_151000_80132084(task);
    func_actor_151000_80132A38(task);
}

/// Exit callback the spawn handler installs on the enemy's task: tears down
/// the enemy the task was spawned for.
void func_actor_151000_801324D4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `stepRec` so each one fires once.
void func_actor_151000_801324FC(Task* task)
{
    Actor151000Work* work;
    GsCOORDINATE2*   obj;
    GpAnimRec*       rec;
    s32              kind;
    s32              id;
    s32              pan;

    work = (Actor151000Work*)task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRec) {
        return;
    }
    work->stepRec = rec;
    kind          = rec->flags & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}

/// Ticks animation slots 1..0x12 of the enemy's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_151000_8013D37C->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a
/// reset argument, and latches the clip into `st.appliedAnimId`. Clears the footstep
/// check's record first.
void func_actor_151000_80132610(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        D_actor_151000_8013D37C->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_151000_8013D37C->rig.anim, i, D_actor_151000_8013D37C->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->st.appliedAnimId = D_actor_151000_8013D37C->st.animId;
}

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_151000_8013D2AC` as the reset argument, and latches the clip into
/// `st.appliedAnimId`. Clears the footstep check's record first.
void func_actor_151000_801326AC(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        func_800B4114(&D_actor_151000_8013D37C->rig.anim, i, D_actor_151000_8013D37C->st.animId, 0,
                      D_actor_151000_8013D2AC);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->st.appliedAnimId = D_actor_151000_8013D37C->st.animId;
}

/// "Start animation" opcode: `withArg` selects between the two start paths the
/// runner `func_actor_151000_80132084` dispatches on, and only the first carries
/// `animArg`, which it leaves in `D_actor_151000_8013D2AC`. The runner is then
/// run once on the task published in `D_actor_151000_8013D380`. Returns -1,
/// without touching the work block, when the clip id is 0x23 or more.
s32 func_actor_151000_80132738(Task* task, s32 arg1, GpAnimArg* args, s32 arg3)
{
    if (args->field_4 < 0x23) {
        D_actor_151000_8013D37C->st.animId = args->field_4;
        if (args->field_8 != 0) {
            D_actor_151000_8013D37C->st.state = 1;
            D_actor_151000_8013D2AC           = args->field_C;
        } else {
            D_actor_151000_8013D37C->st.state = 2;
        }
        D_actor_151000_8013D37C->st.field_6 = 0;
        func_actor_151000_80132084(D_actor_151000_8013D380);
        return 0;
    }
    return -1;
}

/// Visibility opcode: applies `arg2` to the model of the task published in
/// `D_actor_151000_8013D380` - bit 0 shows it (flags 0) rather than hiding it
/// (0x80), and bit 1 ORs in 0x4.
s32 func_actor_151000_801327C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_151000_8013D380->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}

/// Placement opcode: yaws the model's root coordinate to `placement->rot.vy`,
/// caching that yaw in the work block, then drops the placement translation
/// into the matrix and marks it dirty.
s32 func_actor_151000_80132810(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_151000_8013D37C->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler: message 0 arms the turn countdown `turnFrames` at 0x14
/// frames, message 1 sets `footsteps`, which turns the footsteps on. Anything else does nothing.
s32 func_actor_151000_8013288C(Task* task, s32 arg1, GpCmdArg* msg)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            D_actor_151000_8013D37C->turnFrames = 0x14;
            break;
        case 1:
            D_actor_151000_8013D37C->footsteps = kind;
            break;
    }
    return 0;
}

/// "Walk to" opcode: records `mode` in `D_actor_151000_8013D384`, turns the
/// model to face `target` (away from it in mode 1) caching the yaw in the work
/// block, and leaves in `travel` the planar distance divided by the walk's
/// frame count: 0x3C in mode 0, 0xF in mode 1 and 0x19 in mode 2.
s32 func_actor_151000_801328DC(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_151000_8013D384 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_151000_8013D384 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_151000_8013D384) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Draws the enemy's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root part's `workm`
/// translation is staged in a scratchpad VECTOR3 rather than on the stack, and
/// the quad takes the room's current ground shade.
void func_actor_151000_80132A38(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}
