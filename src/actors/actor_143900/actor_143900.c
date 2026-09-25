#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the overlay's first actor variant.
///
/// That variant's spawn routine, `func_actor_143900_80131E70`, allocates it
/// with `memCalloc(0x4F0, 0)` and stores the pointer both in
/// `D_actor_143900_801496B8` and in the task's `Task::work` slot, so the size
/// below is the allocation and not a guess. Every other function of the
/// variant reaches the block through the global.
///
/// `light` and `color` are the two matrices the block supplies to the model:
/// the spawn routine points the object's `lightMtx` / `colorMtx` at them.
/// `anim` is the animation context the tick and reseed loops walk, and `slots`
/// and `pad_374` are what `func_800B3F84` fills in beside it.
typedef struct Actor143900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode `func_actor_143900_80132624` selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_143900_80132624` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ s16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[2];
    /* 0x4EA */ s16        field_4EA; // steps left in the walk the update performs
    /* 0x4EC */ s16        field_4EC; // turn steps left, latched by the 0x7DB handler
    /* 0x4EE */ byte       pad_4EE[2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

/// Payload the overlay's 0x7DB message handlers take as `Gp_DispatchMsg`'s
/// `arg2`; both read only the halfword at 0x2.
typedef struct Actor143900Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor143900Msg;
STATIC_ASSERT_SIZEOF(Actor143900Msg, 0x4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form.
extern TaskDesc D_8017DA00;

/// The first variant's work block, published by its dispatcher
/// `func_actor_143900_80132324` and its spawn routine.
extern Actor143900Work* D_actor_143900_801496B8;

/// The first variant's task, published by its spawn routine so the
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496BC;

/// Approach mode the last `func_actor_143900_8013279C` call selected; the first
/// variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496C0;

/// Reset argument the first variant forwards to the reseed: its play-animation
/// handler latches the preset's `field_C` here, and the update sets it to 10
/// when a walk ends.
extern s16 D_actor_143900_801413B8;

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern GpMsgEntry D_actor_143900_801413BC[];

/// Animation stream the first variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_801413F8[];

/// The second variant's work block, published by its dispatcher
/// `func_actor_143900_80132DEC` and its spawn routine.
extern Actor461800Work* D_actor_143900_801496C4;

/// The second variant's task, published by its spawn routine so the placement,
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496C8;

/// Approach mode the last `func_actor_143900_801333C4` call selected; the
/// second variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496CC;

/// Reset argument the second variant forwards to the reseed: its
/// play-animation handler latches the preset's `field_C` here, and the update
/// sets it to 10 when a walk ends.
extern s16 D_actor_143900_80149630;

/// The second variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern GpMsgEntry D_actor_143900_80149634[];

/// Spawn table the second variant's spawn routine starts its two helper tasks
/// from, indices 1 and 2; the tasks are parked in `field_4F0` / `field_4F4`.
extern TaskDesc D_actor_143900_80149664[];

/// Animation stream the second variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_80149688[];

void func_actor_143900_80131FD4(Task* task);
void func_actor_143900_80132380(GpEnemy* enemy, Task* task);
void func_actor_143900_80132404(Task* task);
void func_actor_143900_8013242C(Task* task);
void func_actor_143900_801324C8(void);
void func_actor_143900_80132514(void);
void func_actor_143900_801325A4(void);
void func_actor_143900_80132A9C(Task* task);
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task);
void func_actor_143900_80132ECC(Task* task);
void func_actor_143900_80132F14(Task* task);
void func_actor_143900_80133068(void);
void func_actor_143900_801330B4(void);
void func_actor_143900_80133144(void);

/// Arms `sceneEvent` and starts the room's spawn-table task, unless
/// `demoScene` is 9, so this story trigger is skipped while the attract demo
/// plays.
void func_actor_143900_80131E24(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.sceneEvent = 0x14;
        Task_SpawnFromTable(&D_8017DA00, 0, 0, 0);
    }
}

/// Spawn routine of the first variant (state 0 of `func_actor_143900_80132324`):
/// allocates the work block and publishes it in `D_actor_143900_801496B8` and
/// the task's `work` slot, binds the model's coordinate to the view and hands
/// the object its light and colour matrices out of the block, publishes the
/// task in `D_actor_143900_801496BC`, relights the model from a point 0x320
/// above its translation, binds the animation stream and runs the first update
/// with the reset mode 2 / id 1 it seeds.
///
/// Every access to the block after the allocation goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use instead of staying in a callee-saved register.
void func_actor_143900_80131E70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F0, 0);
    D_actor_143900_801496B8 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_143900_80132404;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &D_actor_143900_801496B8->light;
    obj->colorMtx           = &D_actor_143900_801496B8->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_143900_801496BC = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496B8->anim, D_actor_143900_801413F8, obj,
                  &D_actor_143900_801496B8->pad_374, D_actor_143900_801496B8->slots);
    D_actor_143900_801496B8->field_4B8 = 1;
    D_actor_143900_801496B8->field_4B4 = 2;
    D_actor_143900_801496B8->field_4EA = 0;
    D_actor_143900_801496B8->field_4EC = 0;
    task->msgTable                     = D_actor_143900_801413BC;
    func_actor_143900_80131FD4(task);
    task->state += 1;
}

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `field_4EA`
/// counts down (distance picked by `D_actor_143900_801496C0`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation.
void func_actor_143900_80131FD4(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor143900Work* work  = (Actor143900Work*)task->work;

    if (D_actor_143900_801496B8->field_4B4 == 1) {
        func_actor_143900_801325A4();
        D_actor_143900_801496B8->field_4B4 = 3;
    } else if (D_actor_143900_801496B8->field_4B4 == 2) {
        func_actor_143900_80132514();
        D_actor_143900_801496B8->field_4B4 = 3;
    } else if (D_actor_143900_801496B8->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_143900_801496C0) {
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
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_143900_801413B8 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_143900_801324C8();
    }
}

/// Two-state dispatcher of the first variant: publishes the task's work block
/// in `D_actor_143900_801496B8` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132324(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_80131E70,
        func_actor_143900_80132380,
    };

    D_actor_143900_801496B8 = (Actor143900Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Second state of the first variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_143900_80132380(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_143900_80131FD4(task);
    func_actor_143900_8013242C(task);
}

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
void func_actor_143900_80132404(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the first variant's ground shadow quad under the model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's brightness
/// follows the room's current ground shade.
void func_actor_143900_8013242C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks animation slots 1..0x13 of the first variant's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496B8->anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the first variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
void func_actor_143900_80132514(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496B8->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496B8->anim, i, D_actor_143900_801496B8->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->field_4B6 = D_actor_143900_801496B8->field_4B8;
}

/// Reseeds animation slots 1..0x13 of the first variant's work block from the
/// current animation id with the latched reset argument, and records that id as
/// the one now playing.
void func_actor_143900_801325A4(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496B8->anim, i, D_actor_143900_801496B8->field_4B8, 0,
                      D_actor_143900_801413B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->field_4B6 = D_actor_143900_801496B8->field_4B8;
}

/// Message 0x7D3 handler of the first variant: adopts `preset`'s animation id
/// when it is one of the first 0x14, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_80132624(Task* task, s32 arg1, GpAnimArg* preset)
{
    if (preset->field_4 < 0x14) {
        D_actor_143900_801496B8->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_143900_801496B8->field_4B4 = 1;
            D_actor_143900_801413B8            = preset->field_C;
        } else {
            D_actor_143900_801496B8->field_4B4 = 2;
        }
        D_actor_143900_801496B8->field_4BA = 0;
        func_actor_143900_80131FD4(D_actor_143900_801496BC);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the first variant: applies `arg2` to the model of the
/// task published in `D_actor_143900_801496BC` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_143900_801326B4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_143900_801496BC->extra;
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

/// Message 0x7D4 handler of the first variant: seeds the task's `TmdObject`
/// coordinate frame from `placement`. Only the yaw is used, remembered in the
/// work block and applied with `Gfx_RotMatrixY`, then the three longs become
/// the coordinate's translation.
s32 func_actor_143900_801326FC(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_143900_801496B8->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler of the first variant: when the payload's halfword at
/// 0x2 is zero, starts a 0x14-step turn, which the update performs while the
/// model plays animation 3.
s32 func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg)
{
    if (msg->field_2 == 0) {
        D_actor_143900_801496B8->field_4EC = 0x14;
    }
    return 0;
}

/// Message 0x7DD handler of the first variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_8013279C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor143900Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor143900Work*)task->work;
    D_actor_143900_801496C0 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_143900_801496C0 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496C0) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}

/// Spawn routine of the second variant (state 0 of `func_actor_143900_80132DEC`):
/// allocates the 0x4F8 work block and publishes it in `D_actor_143900_801496C4`
/// and the task's `work` slot, binds the model's coordinate to the view and
/// hands the object its light and colour matrices out of the block, publishes
/// the task in `D_actor_143900_801496C8`, relights the model from a point 0x320
/// above its translation and binds the animation stream. It then starts the two
/// helper tasks from the overlay's spawn table and runs the first update with
/// the reset mode 2 / id 1 it seeds.
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor461800Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Task*            helper;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, false);
    D_actor_143900_801496C4 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_143900_80132ECC;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.flags       = 1;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    obj->otOffset           = 0x10;
    obj->lightMtx           = &D_actor_143900_801496C4->light;
    obj->colorMtx           = &D_actor_143900_801496C4->color;
    obj->flags              = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    vec.vz                  = coord->workm.t[2];
    D_actor_143900_801496C8 = task;
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496C4->anim, D_actor_143900_80149688, obj,
                  &D_actor_143900_801496C4->pose, D_actor_143900_801496C4->slots);
    D_actor_143900_801496C4->field_4B8 = 1;
    D_actor_143900_801496C4->field_4B4 = 2;
    helper                             = Task_SpawnFromTable(D_actor_143900_80149664, 1, 1, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->field_4F0 = helper;
    }
    helper = Task_SpawnFromTable(D_actor_143900_80149664, 2, 0xC, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->field_4F4 = helper;
    }
    D_actor_143900_801496C4->field_4EA = 0;
    D_actor_143900_801496C4->field_4EC = 0;
    task->msgTable                     = D_actor_143900_80149634;
    func_actor_143900_80132A9C(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `field_4EA`
/// counts down (distance picked by `D_actor_143900_801496CC`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation.
void func_actor_143900_80132A9C(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor461800Work* work  = (Actor461800Work*)task->work;

    if (D_actor_143900_801496C4->field_4B4 == 1) {
        func_actor_143900_80133144();
        D_actor_143900_801496C4->field_4B4 = 3;
    } else if (D_actor_143900_801496C4->field_4B4 == 2) {
        func_actor_143900_801330B4();
        D_actor_143900_801496C4->field_4B4 = 3;
    } else if (D_actor_143900_801496C4->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_143900_801496CC) {
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
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_143900_80149630 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_143900_80133068();
    }
}

/// Two-state dispatcher of the second variant: publishes the task's work block
/// in `D_actor_143900_801496C4` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132DEC(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_801328D4,
        func_actor_143900_80132E48,
    };
    u8 scratch[0x40]; /* never referenced; only reserves the frame */

    D_actor_143900_801496C4 = (Actor461800Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Second state of the second variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_143900_80132A9C(task);
    func_actor_143900_80132F14(task);
}

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`, then kills the two
/// helper tasks the spawn routine started.
void func_actor_143900_80132ECC(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
}

/// Draws the second variant's ground shadow quad under the model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's brightness
/// follows the room's current ground shade.
void func_actor_143900_80132F14(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// Helper-task handler of the second variant: state 0 hangs the task's own
/// coordinate frame off part `spawnArg1` of the second variant's model and
/// steps to state 1; every later tick relights the helper's model from a point
/// 0x320 above that model's root translation.
void func_actor_143900_80132FB0(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_143900_801496C8->extra)->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg      = 0;
            extra->flags    = 0;
            extra->otOffset = 0xF;
            coord->sub      = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Ticks animation slots 1..0x13 of the second variant's animation context.
void func_actor_143900_80133068(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496C4->anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the second variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
void func_actor_143900_801330B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496C4->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496C4->anim, i, D_actor_143900_801496C4->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->field_4B6 = D_actor_143900_801496C4->field_4B8;
}

/// Reseeds animation slots 1..0x13 of the second variant's work block from the
/// current animation id with the latched reset argument, and records that id as
/// the one now playing.
void func_actor_143900_80133144(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496C4->anim, i, D_actor_143900_801496C4->field_4B8, 0,
                      D_actor_143900_80149630);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->field_4B6 = D_actor_143900_801496C4->field_4B8;
}

/// Message 0x7D3 handler of the second variant: adopts `preset`'s animation id
/// when it is one of the first 0xC, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_801331C4(Task* task, s32 arg1, GpAnimArg* preset)
{
    if (preset->field_4 < 0xC) {
        D_actor_143900_801496C4->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_143900_801496C4->field_4B4 = 1;
            D_actor_143900_80149630            = preset->field_C;
        } else {
            D_actor_143900_801496C4->field_4B4 = 2;
        }
        D_actor_143900_801496C4->field_4BA = 0;
        func_actor_143900_80132A9C(D_actor_143900_801496C8);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the second variant: applies `arg2` to the three
/// models it owns - its own task's and the two helper tasks'. Bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 func_actor_143900_80133254(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_143900_801496C8->extra;
    TmdObject* first  = D_actor_143900_801496C4->field_4F0->extra;
    TmdObject* second = D_actor_143900_801496C4->field_4F4->extra;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
    }
    if (arg2 & 2) {
        own->flags    |= 4;
        first->flags  |= 4;
        second->flags |= 4;
    }
    return 0;
}

/// Message 0x7D4 handler of the second variant: seeds the task's `TmdObject`
/// coordinate frame from `placement`. Only the yaw is used, remembered in the
/// work block and applied with `Gfx_RotMatrixY`, then the three longs become
/// the coordinate's translation.
s32 func_actor_143900_801332E4(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_143900_801496C4->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler of the second variant: the payload's halfword at 0x2
/// picks which of the two helper tasks' models is shown - 0 shows the second
/// (`field_4F4`) and hides the first, 1 the reverse; any other value leaves
/// both.
s32 func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg)
{
    TmdObject* first;
    TmdObject* second;

    first  = D_actor_143900_801496C4->field_4F0->extra;
    second = D_actor_143900_801496C4->field_4F4->extra;
    switch (msg->field_2) {
        case 0:
            second->flags = 0;
            first->flags  = 0x80;
            break;
        case 1:
            first->flags  = 0;
            second->flags = 0x80;
            break;
    }
    return 0;
}

/// Message 0x7DD handler of the second variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_801333C4(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor461800Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor461800Work*)task->work;
    D_actor_143900_801496CC = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_143900_801496CC == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496CC) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}
