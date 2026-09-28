#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80132614.h"
#include "actors/actors_shared_801326ac.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "main/fs.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern TaskDesc D_actor_160700_801416A8[];
extern u8       D_actor_160700_801416C0[];
extern u8       D_actor_160700_80141678[];

extern s32 D_actor_160700_801354CC;
extern s32 D_actor_160700_80135664;
extern s32 D_actor_160700_80135ACC;
extern s32 D_actor_160700_80135BD4;
extern s32 D_actor_160700_801362F4;
extern s32 D_actor_160700_80136414;

static void func_actor_160700_80132184(Task* task);
static void func_actor_160700_80132390(GpEnemy* enemy, Task* task);
static void func_actor_160700_80132414(Task* task);
static void func_actor_160700_8013243C(Task* task);
static void func_actor_160700_801324C8(Task* task);
static void func_actor_160700_80132514(Task* task);
static void func_actor_160700_8013258C(Task* task);

static void func_actor_160700_80131E24(void)
{
    Task* slot;

    if (GameFlag_GetNibble(0x113) != 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            Gp_DispatchMsgPtr(slot, 0x7D3, &D_actor_160700_801354CC, 0);
        }
    }
}

static void func_actor_160700_80131E70(void)
{
    switch (GameFlag_GetNibble(0x113)) {
        case 0:
            func_800E8634(&D_actor_160700_80135664, 0, &D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xC);
            break;
        case 1:
            func_800E8634(&D_actor_160700_80135BD4, 0, &D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_160700_801362F4, 0);
            GameFlag_SetNibble(0x113, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_160700_80136414, 0);
            break;
    }
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 1, installs the message table whose handlers are
/// the actor's script opcodes, and starts the animation.
static void func_actor_160700_80131F70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor160600Work* work;
    Actor160600Work* mem;
    GpCoord*         coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor160600Work*)memCalloc(0x4F8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_160700_80132414;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_160700_801416A8, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask  = spawned->task;
    work->st.animId = 1;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_160700_801416C0, obj,
                  work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_160700_80141678;
    func_actor_160700_80132184(task);
    task->state += 1;
}

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
static void func_actor_160700_80132184(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        func_actor_160700_8013258C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_160700_80132514(task);
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
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_160700_801324C8(task);
        return;
    }
}

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_160700_8013233C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_160700_80131F70,
        func_actor_160700_80132390,
    };

    fns[task->state](task->spawnArg2, task);
}

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
static void func_actor_160700_80132390(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 800;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_160700_80132184(task);
    func_actor_160700_8013243C(task);
}

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
static void func_actor_160700_80132414(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model's
/// `flags` bit 0x80 (hidden) is set or it has no buffer. The position is the
/// root part's world translation, staged on the scratchpad stack.
static void func_actor_160700_8013243C(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13.
static void func_actor_160700_801324C8(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
static void func_actor_160700_80132514(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
static void func_actor_160700_8013258C(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: plays clip `args->field_4` (ids from 0x19 up are refused
/// with -1). With `args->field_8` set the slots are reseeded with
/// `args->field_C`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_160700_801325F0(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->field_4 < 0x19) {
        work->st.animId = args->field_4;
        if (args->field_8 != 0) {
            work->st.state = 1;
            work->animArg  = args->field_C;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_160700_80132184(task);
        return 0;
    }
    return -1;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 makes both visible
/// (`TmdObject::flags` 0) and its absence hides them (0x80); bit 1 also sets
/// 0x4. The middle argument is the one every opcode of the table receives.
s32 func_actor_160700_8013265C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor160600Work*)task->work)->pairTask->extra.tmd;

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

/// Script opcode: yaws the actor's root coordinate to `placement->rot.vy`,
/// caching the yaw in the work block, and moves it to `placement->pos`.
s32 func_actor_160700_801326C0(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    Actor160600Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode (message 0x7DB) that does nothing.
s32 func_actor_160700_80132738(void)
{
    return 0;
}

/// Script opcode "walk to": turns the actor's root coordinate to face
/// `target` horizontally, caching the yaw, and stores the horizontal distance
/// in steps of 12 as `travel` for the step body to walk off.
s32 func_actor_160700_80132740(Task* task, s32 arg1, GpXformArg* target)
{
    GpCoord*         coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `flg` so it is recomputed from that part.
void func_actor_160700_80132808(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GpCoord*         coord  = obj->coords;
    GpCoord*         sub    = &parent->extra.tmd->coords[4];
    Actor160600Work* work   = (Actor160600Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->flags    = 0;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
