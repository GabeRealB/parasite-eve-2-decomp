#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"

/// Work block of the actor `func_actor_451100_801322D4` dispatches, published
/// by its spawn handler so the actor's message handlers and animation helpers
/// reach it without the task.
extern Actor260500Work* D_actor_451100_8014E744;

/// That same actor's task, stored by its spawn handler for the handlers that
/// need the task but are not given it.
extern Task* D_actor_451100_8014E748;

/// Reset argument the first actor forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

/// Picks the distance `func_actor_451100_80131F84` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_451100_8014E74C;

extern u8         D_actor_451100_8013F740[];
extern GpMsgEntry D_actor_451100_8013F704[];
extern TaskDesc   D_actor_451100_8014E6E4[];
extern GpMsgEntry D_actor_451100_8014E6B4[];
extern u8         D_actor_451100_8014E6FC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_451100_80131F84(Task* task);
void func_actor_451100_80132330(GpEnemy* enemy, Task* task);
void func_actor_451100_801323B4(Task* task);
void func_actor_451100_801323DC(void);
void func_actor_451100_80132428(void);
void func_actor_451100_801324B8(void);
void func_actor_451100_8013280C(Task* task);
void func_actor_451100_80132A1C(Task* task);
void func_actor_451100_80132C28(GpEnemy* enemy, Task* task);
void func_actor_451100_80132CAC(Task* task);
void func_actor_451100_80132CD4(Task* task);
void func_actor_451100_80132D70(Task* task);
void func_actor_451100_80132DBC(Task* task);
void func_actor_451100_80132E34(Task* task);

/// State 0 of the `func_actor_451100_801322D4` dispatcher: allocates the work
/// block, publishes it in `D_actor_451100_8014E744` and on the task's work
/// slot, points the model's light and color matrices and its animation context
/// at it, then runs the per-frame update once and advances the task to state 1.
///
/// Every access to the block after the null check goes through
/// `D_actor_451100_8014E744` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
void func_actor_451100_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor260500Work* work;
    TmdObject*       obj;
    GpCoord*         coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4B8, 0);
    D_actor_451100_8014E744 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_451100_801323B4;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_451100_8014E744->light;
    obj->colorMtx                = &D_actor_451100_8014E744->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_451100_8014E748      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_451100_8014E744->rig.anim, D_actor_451100_8013F740, obj,
                  D_actor_451100_8014E744->rig.poses, D_actor_451100_8014E744->rig.slots);
    D_actor_451100_8014E744->st.animId  = 0x14;
    D_actor_451100_8014E744->st.state   = 2;
    D_actor_451100_8014E744->st.travel  = 0;
    D_actor_451100_8014E744->turnFrames = 0;
    task->msgTable                      = D_actor_451100_8013F704;
    func_actor_451100_80131F84(task);
    task->state += 1;
}

/// Step routine of the actor `func_actor_451100_801322D4` dispatches, run each
/// frame and by its "start animation" opcode: states 1 and 2 run their one-shot
/// animation reseed and leave the work block in state 3; state 3 walks the
/// model while `travel` counts down in clips 0xE, 2 and 0xF (stride picked by
/// `D_actor_451100_8014E74C`), dropping to clip 0xD with a reset argument of 10
/// when it runs out, turns it while `turnFrames` counts down in clip 3, then ticks
/// the animation.
void func_actor_451100_80131F84(Task* task)
{
    GpCoord*         coord = task->extra.tmd->coords;
    Actor260500Work* work  = (Actor260500Work*)task->work;

    if (D_actor_451100_8014E744->st.state == 1) {
        func_actor_451100_801324B8();
        D_actor_451100_8014E744->st.state = 3;
    } else if (D_actor_451100_8014E744->st.state == 2) {
        func_actor_451100_80132428();
        D_actor_451100_8014E744->st.state = 3;
    } else if (D_actor_451100_8014E744->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_451100_8014E74C) {
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
                    D_actor_451100_8013F700 = 10;
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
        func_actor_451100_801323DC();
    }
}

/// Task handler of the actor whose work block this overlay publishes: runs
/// the handler for the task's state from a two-entry table built on the stack
/// (0 spawns, 1 runs a frame), refreshing `D_actor_451100_8014E744` from the
/// task's work slot first so the handlers can reach the block without the
/// task.
void func_actor_451100_801322D4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_80131E24,
        func_actor_451100_80132330,
    };

    D_actor_451100_8014E744 = (Actor260500Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the `func_actor_451100_801322D4` dispatcher, run each frame:
/// refreshes the model root's world matrix, relights the model from a point
/// 0x320 above its translation, then runs the step routine and draws the
/// ground shadow.
void func_actor_451100_80132330(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_451100_80131F84(task);
    func_actor_451100_8013280C(task);
}

/// Exit callback the `func_actor_451100_801322D4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
void func_actor_451100_801323B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Ticks animation slots 1..0x12 of the published work block, the last step of
/// `func_actor_451100_80131F84`'s state 3.
void func_actor_451100_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_451100_8014E744->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending and reseeds each of them
/// from the current animation id, then records that id as the one now
/// playing. Reaches the block through `D_actor_451100_8014E744`.
void func_actor_451100_80132428(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_451100_8014E744->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_451100_8014E744->rig.anim, i, D_actor_451100_8014E744->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_451100_8014E744->st.appliedAnimId = D_actor_451100_8014E744->st.animId;
}

/// Restarts the animation with the reset argument in
/// `D_actor_451100_8013F700`, the step routine's state 1: reseeds animation
/// slots 1..0x12 from the current animation id and records that id as the one
/// now playing.
void func_actor_451100_801324B8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_451100_8014E744->rig.anim, i, D_actor_451100_8014E744->st.animId, 0,
                      D_actor_451100_8013F700);
        i++;
    } while (i < 0x13);
    D_actor_451100_8014E744->st.appliedAnimId = D_actor_451100_8014E744->st.animId;
}

/// Script opcode: start animation `args->field_4` on this actor through
/// `func_actor_451100_80131F84`, the step routine of the actor whose block is
/// published in `D_actor_451100_8014E744`.
///
/// The same argument block and the same two-way `withArg` start as
/// `func_actor_451100_80132E98`; only the accepted id range (0x25 instead of
/// 0x12) and the run entry point differ. The start argument goes to the overlay's reset
/// word instead of the work block's own slot.
s32 func_actor_451100_80132538(Task* task, s32 arg1, GpAnimArg* args)
{
    if (args->field_4 < 0x25) {
        D_actor_451100_8014E744->st.animId = args->field_4;
        if (args->field_8 != 0) {
            D_actor_451100_8014E744->st.state = 1;
            D_actor_451100_8013F700           = args->field_C;
        } else {
            D_actor_451100_8014E744->st.state = 2;
        }
        D_actor_451100_8014E744->st.field_6 = 0;
        func_actor_451100_80131F84(D_actor_451100_8014E748);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of `D_actor_451100_8013F704`: bit 0 of `arg2` clears
/// `TmdObject::flags` on the model of the task in `D_actor_451100_8014E748`,
/// showing it, and its absence sets 0x80, hiding it; bit 1 additionally ORs in
/// 0x4.
s32 func_actor_451100_801325C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_451100_8014E748->extra.tmd;
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

/// Message 0x7D4 handler of `D_actor_451100_8013F704`, the placement opcode:
/// yaws the task's root coordinate to `placement->rot.vy`, caching that yaw in
/// the published work block, then drops the placement translation into the
/// matrix and marks it dirty.
s32 func_actor_451100_80132610(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    u16      yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_451100_8014E744->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler of `D_actor_451100_8013F704`: a zero payload
/// halfword sets the published block's `turnFrames` to 0x14, the count of frames
/// the step routine turns the model while clip 3 plays.
s32 func_actor_451100_8013268C(Task* task, s32 arg1, GpCmdArg* msg)
{
    if (msg->command == 0) {
        D_actor_451100_8014E744->turnFrames = 0x14;
    }
    return 0;
}

/// Message 0x7DD handler of `D_actor_451100_8013F704`, the "walk to" opcode:
/// records `mode` in `D_actor_451100_8014E74C`, turns the model to face
/// `target` (away from it in mode 1) caching the yaw in the work block, and
/// leaves in `travel` the number of frames the step routine needs to cover the
/// planar distance at that mode's stride: 0x3C in mode 0, 0xF in mode 1 and
/// 0x19 in mode 2.
s32 func_actor_451100_801326B0(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GpCoord*         coord;
    Actor260500Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor260500Work*)task->work;
    D_actor_451100_8014E74C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_451100_8014E74C == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_451100_8014E74C) {
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

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_801322D4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet. The root's world translation is
/// staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
void func_actor_451100_8013280C(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

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

/// State 0 of the `func_actor_451100_80132BD4` dispatcher: allocates the
/// actor's 0x4C0-byte `Actor150400Work` block and hangs it off the task, spawns
/// entry 1 of `D_actor_451100_8014E6E4` (the sub-model task
/// `func_actor_451100_801330B0`), hands it to `Task_Reparent` with this task
/// and keeps it in `pairTask`, then seeds the animation and runs the step
/// routine once.
///
/// `memCalloc`'s result goes through an untyped `block` that `work` is copied
/// from: the raw pointer is what the `Task::work` store and the null test read,
/// so it stays a short-lived `$v0` quantity while the typed copy takes the
/// callee-saved home it needs across the calls below. Assigning the call result
/// straight to `work` collapses the two into one pseudo and puts `$s1` in all
/// three places.
void func_actor_451100_801328A8(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GpCoord*         coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    void*            block;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    block      = memCalloc(0x4C0, false);
    work       = (Actor150400Work*)block;
    task->work = (TaskIdMap*)block;
    if (block == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_451100_80132CAC;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_451100_8014E6E4, 1, 0, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_451100_8014E6FC, obj, work->rig.poses,
                  work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_451100_8014E6B4;
    func_actor_451100_80132A1C(task);
    task->state += 1;
}

/// Step routine of the actor `func_actor_451100_80132BD4` dispatches, run each
/// frame and by its "start animation" opcode. States 1 and 2 restart the clip
/// (with and without the reset argument) and advance to 3; state 3 walks the
/// model 0x11 units a frame while clip 4 plays and `travel` is non-zero,
/// dropping back to clip 1 with 0xA in `animArg` when the count runs out, then
/// ticks the animation.
void func_actor_451100_80132A1C(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_451100_80132E34(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_451100_80132DBC(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0x11);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_451100_80132D70(task);
        return;
    }
}

/// Task handler of the actor whose work block lives only on its task, entry 0
/// of `D_actor_451100_8014E6E4`: runs the handler for the task's state from a
/// two-entry table built on the stack (0 spawns, 1 runs a frame), passing the
/// task's `GpEnemy` as well as the task.
void func_actor_451100_80132BD4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_801328A8,
        func_actor_451100_80132C28,
    };

    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the `func_actor_451100_80132BD4` dispatcher, run each frame:
/// refreshes the model root's coordinate, relights the model from a point 800
/// above its translation, then runs the step routine and draws the ground
/// shadow.
void func_actor_451100_80132C28(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_451100_80132A1C(task);
    func_actor_451100_80132CD4(task);
}

/// Exit callback the `func_actor_451100_80132BD4` spawn handler installs on the
/// actor's task: tears down the enemy the task was spawned for.
void func_actor_451100_80132CAC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_80132BD4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet; the same body as
/// `func_actor_451100_8013280C`.
void func_actor_451100_80132CD4(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

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

/// Ticks animation slots 1..0x12 of the task's work block.
void func_actor_451100_80132D70(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending, reseeds each from the
/// current animation id and records that id as the one now playing.
void func_actor_451100_80132DBC(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Restarts the animation with `animArg` as the reset argument, the step
/// routine's state 1, then records the animation id as the one now playing.
void func_actor_451100_80132E34(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: start animation `args->field_4` on this actor.
///
/// `withArg` selects between the two start paths `func_actor_451100_80132A1C`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_451100_80132E98(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->field_4 >= 0x12) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
    func_actor_451100_80132A1C(task);
    return 0;
}

/// Message 0x7D5 handler of `D_actor_451100_8014E6B4`: bit 0 of `flags`
/// clears `TmdObject::flags` on both the actor's model and its `pairTask`'s,
/// showing them, and its absence sets 0x80, hiding them; bit 1 additionally
/// ORs in 0x4.
s32 func_actor_451100_80132F04(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor150400Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Message 0x7D4 handler of `D_actor_451100_8014E6B4`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw
/// in the work block, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_451100_80132F68(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler of `D_actor_451100_8014E6B4`: accepts the message and
/// does nothing.
s32 func_actor_451100_80132FE0(void)
{
    return 0;
}

/// Message 0x7DD handler of `D_actor_451100_8014E6B4`, the "walk to" opcode:
/// turns the actor's root coordinate to face `target`, caching the yaw in the
/// work block, and leaves the horizontal distance to it, in seventeenths, in
/// `travel` for the step routine to count down.
s32 func_actor_451100_80132FE8(Task* task, s32 arg1, VECTOR* target)
{
    GpCoord*         coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

/// Per-frame handler of the sub-model task, entry 1 of
/// `D_actor_451100_8014E6E4`, reached with the sub-model's own `TmdObject` in
/// `Task::extra` and the actor holding it as `Task::parent`. On its first tick
/// it lights the sub-model with the parent's two leading work matrices and
/// hangs its root coordinate off coordinate 8 of the parent's model; after that
/// it only marks the coordinate dirty each frame so it follows that part.
void func_actor_451100_801330B0(Task* task)
{
    char       pad[0x10];
    Task*      parent = task->parent;
    TmdObject* obj    = task->extra.tmd;
    GpCoord*   coord  = obj->coords;
    GpCoord*   sub    = &parent->extra.tmd->coords[8];
    MATRIX*    work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
