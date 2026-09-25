#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/rand.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_80072729;

extern u8 D_actor_160600_8013DF70[];
extern u8 D_actor_160600_8013DFAC[];
extern u8 D_actor_160600_8013DFEC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Work block the actor's spawn routine allocates with `memCalloc(0x4F8, 0)`
/// and hangs off its task's `Task::work` slot. `light` / `color` are the
/// matrix pair the model is lit with, and `anim`, `slots` and `field_374` are
/// what `func_800B3F84` fills in.
///
/// `state` selects what the step body does next: 1 and 2 reseed the animation
/// slots from `animId` (with and without `animArg`) and advance to 3, which
/// ticks them. `appliedAnimId` records the id the slots were last seeded with.
/// `animId` 4 is the walk clip, which `travel` counts down. `yaw` caches the
/// heading the placement and walk-to opcodes last gave the root coordinate.
/// `field_4EE` nonzero enables the per-frame effect spawns. `pairTask` is the
/// task whose model the visibility opcode drives alongside the actor's own when
/// `Task::spawnArg1` is set; nothing in this actor stores it, so it keeps the
/// allocation's zero. `enemy` is the enemy the actor's task belongs to.
typedef struct Actor160600Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ Task*      pairTask;
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor160600Work;
STATIC_ASSERT_SIZEOF(Actor160600Work, 0x4F8);

/// Payload of the "play animation" script opcode: which clip to play, and
/// whether to seed the slots with `animArg`.
typedef struct Actor160600AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ s16  animArg;
} Actor160600AnimArgs;

/// Payload of the script opcode that sets the work block's `field_4EE`.
typedef struct Actor160600FlagArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  value;
} Actor160600FlagArgs;

void func_actor_160600_80131FFC(Task* task);
void func_actor_160600_80132208(GpEnemy* enemy, Task* task);
void func_actor_160600_80132350(Task* task);
void func_actor_160600_80132378(Task* task);
void func_actor_160600_80132404(Task* task);
void func_actor_160600_80132450(Task* task);
void func_actor_160600_801324C8(Task* task);

/// Passes the task filed in the session's pointer slot 0xA, if any, to
/// `Task_CallExit` and empties the slot.
void func_actor_160600_80131E24(void)
{
    if (gameGetPtrSlot(0xA) != NULL) {
        Task_CallExit(gameGetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}

/// The actor's per-frame body (task state 1): refreshes the root coordinate,
/// re-lights the model at the root translation raised by 800, then runs the
/// step body and draws the ground shadow. While `field_4EE` is set and the
/// model is shown and has a buffer, every other frame spawns effect 0x60070 on
/// a randomly chosen part, with two `Gp_LcgState` draws packed into the effect
/// argument.
void func_actor_160600_80131E68(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    Actor160600Work* work;
    VECTOR           pos;
    u32              low;
    u32              high;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    part  = &((TmdObject*)task->extra)->coords[D_actor_160600_8013DFEC[(rand() * 11) >> 15]];
    work  = (Actor160600Work*)task->work;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_160600_80131FFC(task);
    func_actor_160600_80132378(task);
    if (work->field_4EE != 0 && !(obj->flags & 0x80) && obj->buffer != NULL) {
        if (task->killCountdown & 1) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            low         = (Gp_LcgState >> 16) & 0x10FF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            high        = (((Gp_LcgState >> 16) & 1) << 30) + 0x800231C0;
            Gp_SpawnEff(0x60070, part, low + high, NULL);
        }
        task->killCountdown++;
    }
}

/// The actor's step body. States 1 and 2 reseed the animation slots (with and
/// without `animArg`) and advance to 3; state 3 walks the root coordinate 12
/// units per frame while the walk clip has `travel` left, switching to clip 1
/// with argument 0xA when it runs out, then ticks the slots.
void func_actor_160600_80131FFC(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->state == 1) {
        func_actor_160600_801324C8(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_160600_80132450(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            actorMoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_160600_80132404(task);
        return;
    }
}

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `GpEnemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_160600_801321B4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_160600_80132208,
        func_actor_160600_80131E68,
    };

    fns[task->state](task->spawnArg2, task);
}

/// The actor's spawn routine (task state 0): allocates the work block,
/// destroying the enemy if that fails, and installs the exit callback. It then
/// lights the model at its root translation raised by 800, sets up the
/// animation context and the task's message table, and runs the step body
/// once with the plain reseed of clip 10 queued.
void func_actor_160600_80132208(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor160600Work* work;
    Actor160600Work* mem;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor160600Work*)memCalloc(sizeof(Actor160600Work), false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_160600_80132350;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    obj->flags           = 0;
    work->animId         = 10;
    work->enemy          = enemy;
    obj->lightMtx        = &work->light;
    obj->colorMtx        = &work->color;
    vec.vx               = coord->workm.t[0];
    vec.vy               = coord->workm.t[1] - 0x320;
    vec.vz               = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_160600_8013DFAC, obj,
                  &work->field_374, work->slots);
    work->state    = 2;
    task->msgTable = D_actor_160600_8013DF70;
    func_actor_160600_80131FFC(task);
    task->state++;
}

/// The actor's `Task::exitCallback`: hands the task's `GpEnemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
void func_actor_160600_80132350(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`TmdObject::flags` bit 0x80) or has no buffer yet. The world
/// position is the translation of the root coordinate's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack.
void func_actor_160600_80132378(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_160600_80132404(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Reseeds animation slots 1..0x13 with `animId`, each at rate 1, and records
/// that id as the one applied.
void func_actor_160600_80132450(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Reseeds animation slots 1..0x13 with `animId`, passing `animArg` through,
/// and records that id as the one applied.
void func_actor_160600_801324C8(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths the step body
/// `func_actor_160600_80131FFC` dispatches on, and only the first carries
/// `animArg`. Returns -1, without touching the work block, when the clip id is
/// out of range.
///
/// Both halves of the shape are load-bearing, not stylistic. The two bare
/// `return`s put the constants straight into `$v0`, where the ROM has them; an
/// m2c-style `var_v0` accumulator instead colours them into the free `$a1` and
/// costs a tail `move $v0,$a1` (84.59%). The `SOFT_BARRIER()` moves the second
/// `beqz`'s delay slot from the fall-through arm (`state = 1`) to the else arm
/// (`state = 2`), which is what the ROM has; without it the function is 92.37%.
/// See DECOMPILATION_LEARNINGS.md, "An empty `asm` at the head of the then-arm
/// moves the branch delay slot to the else arm".
s32 func_actor_160600_8013252C(Task* task, s32 arg1, Actor160600AnimArgs* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->animId >= 0x10) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_160600_80131FFC(task);
    return 0;
}

/// Script opcode: shows or hides this actor's model and the model of the task
/// parked in `pairTask`. With `flags` bit 0 both models get `TmdObject::flags`
/// 0, which shows them; without it they get 0x80, which hides them. Bit 1
/// additionally ORs in 0x4. With `Task::spawnArg1` clear the actor drives its
/// own model twice.
s32 func_actor_160600_80132598(Task* task, s32 arg1, s32 flags)
{
    Actor160600Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = (TmdObject*)task->extra;
    work = (Actor160600Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->pairTask->extra;
    } else {
        other = self;
    }
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

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_160600_80132614(Task* task, s32 arg1, GpPlaceArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor160600Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode: sets the work block's `field_4EE`, which enables the
/// per-frame effect spawns, when the payload is exactly 1; any other payload
/// is ignored.
s32 func_actor_160600_8013268C(Task* task, s32 arg1, Actor160600FlagArgs* args)
{
    Actor160600Work* work;
    u16              value;

    value = args->value;
    work  = (Actor160600Work*)task->work;
    if (value == 1) {
        work->field_4EE = value;
    }
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the remaining distance in twelfths as the
/// `travel` the step body's walk counts down.
s32 func_actor_160600_801326AC(Task* task, s32 arg1, GpPlaceArg* target)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor160600Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
