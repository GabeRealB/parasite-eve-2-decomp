#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern TaskDesc D_actor_150400_80132CF0;
extern TaskDesc D_80181BBC;
extern Task*    D_actor_150400_8013C924;
extern Task*    D_actor_150400_8013C928;

extern TaskDesc   D_actor_150400_8013C8F4[];
extern u8         D_actor_150400_8013C90C[];
extern GpMsgEntry D_actor_150400_8013C8C4[];

extern GpAreaApplyRec D_80183BE0;

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_150400_80132228(Task* task);
void func_actor_150400_80132434(GpEnemy* enemy, Task* task);
void func_actor_150400_801324B8(Task* task);
void func_actor_150400_801324E0(Task* task);
void func_actor_150400_8013257C(Task* task);
void func_actor_150400_801325C8(Task* task);
void func_actor_150400_80132640(Task* task);

/// Per-frame callback of the model task `D_actor_150400_80132CF0` describes,
/// spawned twice by `func_actor_150400_80131FB8` with `spawnArg1` 1 and 2.
/// State 0 places the model's coordinate (the two copies differ only in z) and
/// moves on to 1; `func_actor_150400_80131F9C` puts both copies into state 2,
/// which slides them along x by 4 a frame up to 0x406. The model is drawn only
/// while the save's view byte is 5; otherwise its flags are set to 0x84, which
/// hides it.
void func_actor_150400_80131E24(Task* task)
{
    TmdObject* obj   = task->extra.tmd;
    GpCoord*   coord = obj->coords;

    if (task->state == 0) {
        coord->coord.t[0] = 0x2DA;
        coord->coord.t[1] = -0x564;
        if (task->spawnArg1 == 1) {
            coord->coord.t[2] = -0x116C;
        } else {
            coord->coord.t[2] = -0x1018;
        }
        coord->flg = 0;
        task->state++;
    }
    if (task->state == 2) {
        coord->coord.t[0] += 4;
        if (coord->coord.t[0] > 0x406) {
            coord->coord.t[0] = 0x406;
        }
        coord->flg = 0;
    }
    if (Mc_SaveData.at4.loc.view != 5) {
        obj->flags = 0x84;
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

void func_actor_150400_80131ECC(void)
{
    if (Mc_SaveData.demoScene != 9) {
        SetDispMask(1);
        GameFlag_SetNibble(0xE5, 1);
        Gp_EnqueueConfigCd(1);
        Gp_ApplyAreaRecs(&D_80183BE0);
        Mc_SaveData.at4.loc.stage = 4;
        Mc_SaveData.at4.loc.area  = 0x21;
        Mc_SaveData.at4.loc.warp  = 4;
        Mc_SaveData.at4.loc.room  = 1;
        gDisplayState.roomVariant = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Gp_RestoreStreamRng();
    }
}

void func_actor_150400_80131F6C(void)
{
    Task_SpawnFromTable(&D_80181BBC, 0, 0, 0);
}

void func_actor_150400_80131F9C(s32 arg0)
{
    D_actor_150400_8013C924->state = arg0;
    D_actor_150400_8013C928->state = arg0;
}

void func_actor_150400_80131FB8(void)
{
    D_actor_150400_8013C924 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 1, 0);
    D_actor_150400_8013C928 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 2, 0);
}

/// State-0 handler of the actor's task: allocates the work block, starts the
/// sub-model task and parents it under this one, textures the sub-model from
/// the placement record of the current area, then starts the animation in
/// state 2 and runs the step body `func_actor_150400_80132228` once.
///
/// Two codegen pins, both load-bearing. `key` lands at `vfp+0x10`, so `&key` is
/// expensive enough that `expand_call` precomputes it; left alone CSE merges
/// the two call-site copies into one pseudo live across the first call and the
/// address costs a callee-saved register. The `SOFT_BARRIER()` keeps each
/// materialization next to its own call and `TOUCH_REG` makes the second a
/// fresh computation, which is the ROM's `addiu $a0,$sp,0x28` twice. The
/// `mem`/`work` pair is the same kind of pin on the `memCalloc` result: the
/// ROM keeps a short-lived copy for the `work` store, the NULL test and
/// `enemy`, and a longer-lived one for everything after, which one variable
/// cannot express.
void func_actor_150400_80132014(GpEnemy* enemy, Task* task)
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
    mem        = (Actor150400Work*)memCalloc(sizeof(Actor150400Work), false);
    work       = (Actor150400Work*)mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_150400_801324B8;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    mem->enemy                   = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
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
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
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
    func_800B3F84(&work->rig.anim, D_actor_150400_8013C90C, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_150400_8013C8C4;
    func_actor_150400_80132228(task);
    task->state++;
}

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 0x11 units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA when it runs out, then ticks
/// the slots.
void func_actor_150400_80132228(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_150400_80132640(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_150400_801325C8(task);
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
            actorMoveForward(task->extra.tmd->coords, 0x11);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_150400_8013257C(task);
        return;
    }
}

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `func_actor_150400_80132014` in state 0 and the per-frame update
/// `func_actor_150400_80132434` after it, passing the task's `GpEnemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_150400_80132014,
        func_actor_150400_80132434,
    };

    fns[task->state](task->spawnArg2, task);
}

/// State-1 handler of the actor's task, run every frame: refreshes the model
/// root's coordinate, feeds its world translation (raised by 800 on y) to
/// `func_800D7A9C`, then runs the animation step body and draws the ground
/// shadow.
void func_actor_150400_80132434(GpEnemy* enemy, Task* task)
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
    func_actor_150400_80132228(task);
    func_actor_150400_801324E0(task);
}

/// Exit callback of the actor's task: hands its `GpEnemy` back to
/// `Gp_DestroyEnemy`.
void func_actor_150400_801324B8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack, and the shade follows the
/// room's current `Gp_State1C` level.
void func_actor_150400_801324E0(Task* task)
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

/// Ticks animation slots 1..0x12.
void func_actor_150400_8013257C(Task* task)
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

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
void func_actor_150400_801325C8(Task* task)
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

/// Reseeds animation slots 1..0x12 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_150400_80132640(Task* task)
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
/// `withArg` selects between the two start paths `func_actor_150400_80132228`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_150400_801326A4(Task* task, s32 arg1, GpAnimArg* args)
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
    func_actor_150400_80132228(task);
    return 0;
}

/// Script opcode: set the visibility of the actor's model and of its sub-model
/// (the model of the task in `pairTask`) together. `flags` bit 0 shows both
/// (`TmdObject::flags` = 0) and its absence hides them (0x80); bit 1
/// additionally sets bit 0x4 on both.
s32 func_actor_150400_80132710(Task* task, s32 arg1, s32 flags)
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

/// Script opcode: place the actor. Yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `yaw`, then drops the placement
/// translation into the matrix and marks it for recomputation.
s32 func_actor_150400_80132774(Task* task, s32 arg1, GpXformArg* placement)
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

s32 func_actor_150400_801327EC(void)
{
    return 0;
}

/// Script opcode: walk to `target`. Aims the actor's root coordinate at it by
/// the yaw of the horizontal offset from the coordinate's own translation,
/// caches that yaw in `yaw` and rebuilds the local matrix from it, then sets
/// `travel` to the distance divided by 17, the step body's per-frame stride.
s32 func_actor_150400_801327F4(Task* task, s32 arg1, GpXformArg* target)
{
    GpCoord*         coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

/// Per-frame callback of the actor's sub-model task, which the spawn handler
/// parents under the actor's own task. On the first frame it draws the
/// sub-model under the actor's `light` / `color` matrices and parents its root
/// coordinate to part 7 of the actor's model, then advances to state 1; from
/// then on it only marks the coordinate for recomputation each frame.
void func_actor_150400_801328BC(Task* task)
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
