#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80132614.h"
#include "actors/actors_shared_801326ac.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern TaskDesc D_actor_160700_801416A8[];
extern u8       D_actor_160700_801416C0[];
extern u8       D_actor_160700_80141678[];

extern s32 D_actor_160700_801354CC;
extern s32 D_actor_160700_80135664;
extern s32 D_actor_160700_80135ACC;
extern s32 D_actor_160700_80135BD4;
extern s32 D_actor_160700_801362F4;
extern s32 D_actor_160700_80136414;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_160700_80132184(Task* task);
void func_actor_160700_80132390(GpEnemy* enemy, Task* task);
void func_actor_160700_80132414(Task* task);
void func_actor_160700_8013243C(Task* task);
void func_actor_160700_801324C8(Task* task);
void func_actor_160700_80132514(Task* task);
void func_actor_160700_8013258C(Task* task);

void func_actor_160700_80131E24(void)
{
    s32 slot;

    if (GameFlag_GetNibble(0x113) != 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            Gp_DispatchMsg((Task*)slot, 0x7D3, (s32)&D_actor_160700_801354CC, 0);
        }
    }
}

void func_actor_160700_80131E70(void)
{
    switch (GameFlag_GetNibble(0x113)) {
        case 0:
            func_800E8634((s32)&D_actor_160700_80135664, 0, (s32)&D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xC);
            break;
        case 1:
            func_800E8634((s32)&D_actor_160700_80135BD4, 0, (s32)&D_actor_160700_80135ACC);
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
void func_actor_160700_80131F70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GpAreaKey        key;
    Actor160700Work* work;
    Actor160700Work* mem;
    GsCOORDINATE2*   coord;
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

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor160700Work*)memCalloc(0x4F8, false);
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
    model                        = (TmdObject*)spawned->task->extra;
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
    work->field_4F0 = spawned->task;
    work->animId    = 1;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_160700_801416C0, obj,
                  &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = D_actor_160700_80141678;
    func_actor_160700_80132184(task);
    task->state += 1;
}

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
void func_actor_160700_80132184(Task* task)
{
    Actor160700Work* work;
    s16              animId;

    work = (Actor160700Work*)task->work;
    if (work->state == 1) {
        func_actor_160700_8013258C(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_160700_80132514(task);
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
void func_actor_160700_80132390(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
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
void func_actor_160700_80132414(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model's
/// `flags` bit 0x80 (hidden) is set or it has no buffer. The position is the
/// root part's world translation, staged on the scratchpad stack.
void func_actor_160700_8013243C(Task* task)
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

/// Ticks animation slots 1..0x13.
void func_actor_160700_801324C8(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
void func_actor_160700_80132514(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->animId);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_160700_8013258C(Task* task)
{
    Actor160700Work* work;
    s32              i;

    work = (Actor160700Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->appliedAnimId = work->animId;
}

/// Script opcode: plays clip `args->field_4` (ids from 0x19 up are refused
/// with -1). With `args->field_8` set the slots are reseeded with
/// `args->field_C`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_160700_801325F0(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160700Work* work;

    work = (Actor160700Work*)task->work;
    if (args->field_4 >= 0x19) {
        return -1;
    }

    work->animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->field_C;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_160700_80132184(task);
    return 0;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 makes both visible
/// (`TmdObject::flags` 0) and its absence hides them (0x80); bit 1 also sets
/// 0x4. The middle argument is the one every opcode of the table receives.
s32 func_actor_160700_8013265C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor160700Work*)task->work)->field_4F0->extra;

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
    GsCOORDINATE2*   coord;
    Actor160700Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor160700Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
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
    GsCOORDINATE2*   coord;
    Actor160700Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor160700Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
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
    TmdObject*       obj    = task->extra;
    GsCOORDINATE2*   coord  = obj->coords;
    GsCOORDINATE2*   sub    = &((TmdObject*)parent->extra)->coords[4];
    Actor160700Work* work   = (Actor160700Work*)parent->work;

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
