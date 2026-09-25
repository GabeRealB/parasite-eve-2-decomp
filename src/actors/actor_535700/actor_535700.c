#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"

/// The first enemy's work block, published by its spawn handler.
extern Actor151000Work* D_actor_535700_80146844;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* D_actor_535700_80146848;

/// Fade countdown. `func_actor_535700_80131EF0` seeds it from its argument and
/// spawns the fade task from `D_actor_535700_8013346C`; that task
/// (`func_actor_535700_80131E24`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

/// Reset argument the first enemy's "play animation" opcode leaves behind:
/// `func_actor_535700_80132730` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 D_actor_535700_8013DAA8;

/// Picks the distance `func_actor_535700_80132108` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_535700_8014684C;

/// Descriptor of the fade task `func_actor_535700_80131E24`.
extern TaskDesc D_actor_535700_8013346C;

/// The first enemy's message table and the animation data its work block's
/// slots are seeded from.
extern GpMsgEntry D_actor_535700_8013DAAC[];
extern u8         D_actor_535700_8013DAE8[];

/// The second enemy's message table, the `TaskDesc` table its sub-model task
/// comes from, and the animation data its work block's slots are seeded from.
extern GpMsgEntry D_actor_535700_801467E0[];
extern TaskDesc   D_actor_535700_80146810[];
extern u8         D_actor_535700_80146828[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_535700_80132108(Task* task);
void func_actor_535700_801324D4(GpEnemy* enemy, Task* task);
void func_actor_535700_80132558(Task* task);
void func_actor_535700_80132580(Task* task);
void func_actor_535700_80132648(void);
void func_actor_535700_80132694(void);
void func_actor_535700_80132730(void);
void func_actor_535700_80132ABC(Task* task);
void func_actor_535700_80132D68(Task* task);
void func_actor_535700_80132F74(GpEnemy* enemy, Task* task);
void func_actor_535700_80132FF8(Task* task);
void func_actor_535700_80133020(Task* task);
void func_actor_535700_801330BC(Task* task);
void func_actor_535700_80133108(Task* task);
void func_actor_535700_80133180(Task* task);

/// The fade task: while `D_actor_535700_80146840` is non-zero, draws a
/// full-screen black `TILE` into ordering table slot 0xA; once it reaches zero
/// the task kills itself. The count drops by one every frame.
void func_actor_535700_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_535700_80146840 != 0) {
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
    D_actor_535700_80146840--;
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_535700_80131EF0(s32 frames)
{
    D_actor_535700_80146840 = frames;
    if (frames != 0) {
        Task_SpawnFromTable(&D_actor_535700_8013346C, 0, 0, 0);
    }
}

void func_actor_535700_80131F2C(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.at4.loc.area  = 0x1D;
        Mc_SaveData.at4.loc.warp  = 5;
        Mc_SaveData.at4.loc.room  = 2;
        gDisplayState.roomVariant = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Mc_SaveData.sceneEvent = 6;
        Gp_RestoreStreamRng();
    }
}

/// State 0 of the first enemy's task: allocates the work block, publishes it
/// in `D_actor_535700_80146844` and on the task's work slot, points the
/// model's light and colour matrices and its animation context at it,
/// publishes the task in `D_actor_535700_80146848`, then runs the runner once
/// and advances the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
void func_actor_535700_80131FA0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
    TmdObject*       obj;
    GpCoord*         coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_535700_80146844 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_535700_80132558;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_535700_80146844->light;
    obj->colorMtx                = &D_actor_535700_80146844->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_535700_80146848      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_535700_80146844->rig.anim, D_actor_535700_8013DAE8, obj,
                  &D_actor_535700_80146844->rig.poses, D_actor_535700_80146844->rig.slots);
    D_actor_535700_80146844->st.animId  = 1;
    D_actor_535700_80146844->st.state   = 2;
    D_actor_535700_80146844->st.travel  = 0;
    D_actor_535700_80146844->turnFrames = 0;
    D_actor_535700_80146844->stepRec    = 0;
    D_actor_535700_80146844->footsteps  = 0;
    task->msgTable                      = D_actor_535700_8013DAAC;
    func_actor_535700_80132108(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `st.travel`
/// counts down (distance picked by `D_actor_535700_8014684C`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation and, once
/// `footsteps` is set, plays the footsteps.
void func_actor_535700_80132108(Task* task)
{
    GpCoord*         coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_535700_80146844->st.state == 1) {
        func_actor_535700_80132730();
        D_actor_535700_80146844->st.state = 3;
    } else if (D_actor_535700_80146844->st.state == 2) {
        func_actor_535700_80132694();
        D_actor_535700_80146844->st.state = 3;
    } else if (D_actor_535700_80146844->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_535700_8014684C) {
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
                    D_actor_535700_8013DAA8 = 10;
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
        func_actor_535700_80132648();
        if (work->footsteps != 0) {
            func_actor_535700_80132580(task);
        }
    }
}

/// The first enemy's task body: publishes the task's work block in
/// `D_actor_535700_80146844`, then runs the handler for the task's state from
/// a table built on the stack - the spawn handler `func_actor_535700_80131FA0`,
/// then the per-frame `func_actor_535700_801324D4`.
void func_actor_535700_80132478(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80131FA0,
        func_actor_535700_801324D4,
    };

    D_actor_535700_80146844 = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the first enemy's task: refreshes the model root's coordinate,
/// hands `func_800D7A9C` the point 0x320 above it, then runs the runner and
/// draws the ground shadow.
void func_actor_535700_801324D4(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_535700_80132108(task);
    func_actor_535700_80132ABC(task);
}

/// Exit callback the first enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void func_actor_535700_80132558(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `stepRec` so each one fires once.
void func_actor_535700_80132580(Task* task)
{
    Actor151000Work* work;
    GpCoord*         obj;
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

/// Ticks animation slots 1..0x12 of the first enemy's animation context.
void func_actor_535700_80132648(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_535700_80146844->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reset
/// argument, and latches the clip into `st.appliedAnimId`. Clears the footstep
/// check's record first.
void func_actor_535700_80132694(void)
{
    s32 i;

    D_actor_535700_80146844->stepRec = NULL;
    i                                = 1;
    do {
        D_actor_535700_80146844->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_535700_80146844->rig.anim, i, D_actor_535700_80146844->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->st.appliedAnimId = D_actor_535700_80146844->st.animId;
}

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_535700_8013DAA8` as the reset argument, and latches the clip into
/// `st.appliedAnimId`. Clears the footstep check's record first.
void func_actor_535700_80132730(void)
{
    s32 i;

    D_actor_535700_80146844->stepRec = 0;
    i                                = 1;
    do {
        func_800B4114(&D_actor_535700_80146844->rig.anim, i, D_actor_535700_80146844->st.animId, 0,
                      D_actor_535700_8013DAA8);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->st.appliedAnimId = D_actor_535700_80146844->st.animId;
}

/// "Start animation" opcode of the first enemy: `withArg` selects between the
/// two start paths the runner `func_actor_535700_80132108` dispatches on, and
/// only the first carries `animArg`, which it leaves in
/// `D_actor_535700_8013DAA8`. Returns -1, without touching the work block, when
/// the clip id is 0x23 or more.
s32 func_actor_535700_801327BC(Task* task, s32 arg1, GpAnimArg* args, s32 arg3)
{
    if (args->field_4 < 0x23) {
        D_actor_535700_80146844->st.animId = args->field_4;
        if (args->field_8 != 0) {
            D_actor_535700_80146844->st.state = 1;
            D_actor_535700_8013DAA8           = args->field_C;
        } else {
            D_actor_535700_80146844->st.state = 2;
        }
        D_actor_535700_80146844->st.field_6 = 0;
        func_actor_535700_80132108(D_actor_535700_80146848);
        return 0;
    }
    return -1;
}

/// Visibility opcode of the first enemy: applies `arg2` to the model of the
/// task published in `D_actor_535700_80146848` - bit 0 shows it (flags 0)
/// rather than hiding it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_535700_8013284C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_535700_80146848->extra.tmd;
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

/// Placement opcode of the first enemy: yaws the model's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block's `st.yaw`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_80132894(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    u16      yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_535700_80146844->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler of the first enemy: message 0 arms the turn countdown
/// `turnFrames` at 0x14 frames, message 1 sets `footsteps`, which turns the
/// footsteps on. Anything else does nothing.
s32 func_actor_535700_80132910(Task* task, s32 arg1, GpCmdArg* msg)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            D_actor_535700_80146844->turnFrames = 0x14;
            break;
        case 1:
            D_actor_535700_80146844->footsteps = kind;
            break;
    }
    return 0;
}

/// "Walk to" opcode of the first enemy: turns the model to face `target` --
/// away from it in mode 1 -- and leaves in `st.travel` the planar distance
/// divided by the walk's frame count: 60 in mode 0, 15 in mode 1 and 25 in
/// mode 2. The mode is kept in `D_actor_535700_8014684C`, which picks the
/// runner's step.
s32 func_actor_535700_80132960(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GpCoord*         coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_535700_8014684C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_535700_8014684C == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_535700_8014684C) {
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

/// Draws the first enemy's ground shadow quad under its model root, unless the
/// model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's shade is
/// the room's current `Gp_State1C` level.
void func_actor_535700_80132ABC(Task* task)
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

/// State 0 of the second enemy's task. Allocates its `Actor150400Work`,
/// spawns its sub-model task from `D_actor_535700_80146810`, takes the
/// sub-model's texture page and CLUT from the placement record the enemy's
/// `placeKey` selects, makes the sub-model a child of this task, lights the
/// model at its world position, starts the animation and runs the state
/// machine `func_actor_535700_80132D68` once.
///
/// Two codegen pins, both load-bearing. Left alone, CSE merges the two
/// call-site copies of `&key` into one pseudo live across the first call,
/// costing a callee-saved register; `SOFT_BARRIER()` keeps each
/// materialization next to its own call and `TOUCH_REG` makes the second a
/// fresh one. The `mem` / `work` pair reproduces the ROM's short-lived copy of
/// the `memCalloc` result beside the long-lived one.
void func_actor_535700_80132B58(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GpAreaKey        key;
    Actor150400Work* work;
    Actor150400Work* mem;
    GpCoord*         coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    TmdObject*       model;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     place;
    s32              idx;
    u32              raw;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor150400Work*)memCalloc(0x4C0, false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_535700_80132FF8;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    mem->enemy                   = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_535700_80146810, 1, 0, enemy);
    model                        = spawned->task->extra.tmd;
    raw                          = enemy->placeKey;
    sessionKey                   = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage                    = sessionKey->stage;
    key.area                     = sessionKey->area;
    key.room                     = sessionKey->room;
    idx                          = raw >> 12;
    areaByte0                    = sessionKey->view;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_535700_80146828, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_535700_801467E0;
    func_actor_535700_80132D68(task);
    task->state++;
}

/// The second enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_535700_80133180` or `func_actor_535700_80133108` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
void func_actor_535700_80132D68(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_535700_80133180(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_535700_80133108(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_535700_801330BC(task);
        return;
    }
}

/// The second enemy's task body: runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_535700_80132B58`,
/// then the per-frame `func_actor_535700_80132F74`.
void func_actor_535700_80132F20(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80132B58,
        func_actor_535700_80132F74,
    };

    fns[task->state](task->spawnArg2, task);
}

/// State 1 of the second enemy's task: refreshes the model root's coordinate,
/// hands `func_800D7A9C` the point 0x320 above it, then runs the state machine
/// and draws the ground shadow.
void func_actor_535700_80132F74(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_535700_80132D68(task);
    func_actor_535700_80133020(task);
}

/// Exit callback the second enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void func_actor_535700_80132FF8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the second enemy's ground shadow quad under its model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's shade is
/// the room's current `Gp_State1C` level.
void func_actor_535700_80133020(Task* task)
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

/// Ticks the second enemy's animation slots 1..0x12.
void func_actor_535700_801330BC(Task* task)
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

/// Resets the second enemy's animation slots 1..0x12 to clip `animId` at rate
/// 1, without a reset argument, and latches the clip into `appliedAnimId`.
void func_actor_535700_80133108(Task* task)
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

/// Starts the second enemy's animation slots 1..0x12 on clip `animId`,
/// forwarding `animArg` as the reset argument, and latches the clip into
/// `appliedAnimId`.
void func_actor_535700_80133180(Task* task)
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

/// "Start animation" opcode of the second enemy: `withArg` selects between the
/// two start paths `func_actor_535700_80132D68` dispatches on, and only the
/// first carries `animArg`. Returns -1, without touching the work block, when
/// the clip id is 6 or more.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one: without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`)
/// rather than the else arm's `state = 2`, which the ROM has there.
s32 func_actor_535700_801331E4(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->field_4 >= 6) {
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
    func_actor_535700_80132D68(task);
    return 0;
}

/// Visibility opcode of the second enemy: sets `TmdObject::flags` on its own
/// model and on the sub-model task's in `pairTask` at once. `flags` bit 0
/// shows them (0) rather than hiding them (0x80), and bit 1 ORs 4 in.
s32 func_actor_535700_80133250(Task* task, s32 arg1, s32 flags)
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

/// Placement opcode of the second enemy: yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `Actor150400Work::yaw`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_801332B4(Task* task, s32 arg1, GpXformArg* placement)
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

s32 func_actor_535700_8013332C(void)
{
    return 0;
}

/// "Walk to" opcode of the second enemy: turns its root coordinate to face
/// `target`, caching the yaw in `Actor150400Work::yaw`, and leaves the
/// horizontal distance to it, in twelfths, in `travel` for the walk state to
/// count down.
s32 func_actor_535700_80133334(Task* task, s32 arg1, VECTOR* target)
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
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Task body of the second enemy's sub-model, which the enemy's spawn handler
/// makes a child of the enemy's task. On its first tick it lights the
/// sub-model with the enemy's `Actor150400Work` matrices and hangs its
/// root coordinate off part 7 of the enemy's model; after that it only marks
/// the coordinate dirty each frame so it follows that part.
void func_actor_535700_801333FC(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GpCoord*         coord  = obj->coords;
    GpCoord*         sub    = &parent->extra.tmd->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
