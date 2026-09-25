#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block the spawn handler allocates (`memCalloc(0x4C4)`) and parks in
/// `Task::work`. It opens with the animation context the preset handler hands
/// `func_800B3F84` at the block's own address, the 0x14 0x28-byte slots
/// immediately above it and the 0x140-byte table at 0x334 that call also
/// takes; the slot walkers run to 0x14, the slot count. `field_474` latches
/// once a preset has started the slots and gates the per-frame tick;
/// `field_476` and `field_475` hold the current bank index and animation id,
/// seeded to -1 by the spawn handler. `field_477` is the countdown after which
/// the tick frees the model's buffers, -1 while idle. `light` / `color` are
/// the matrices published on the model as its light and colour matrices.
/// `field_4BC` / `field_4C0` hold the two children spawned from table entries
/// 1 and 2, whose models the 0x7DB handler shows and hides.
typedef struct Actor213000Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ s8             field_474;
    /* 0x475 */ s8             field_475;
    /* 0x476 */ s8             field_476;
    /* 0x477 */ s8             field_477;
    /* 0x478 */ s32            field_478;
    /* 0x47C */ MATRIX         light;
    /* 0x49C */ MATRIX         color;
    /* 0x4BC */ Task*          field_4BC;
    /* 0x4C0 */ Task*          field_4C0;
} Actor213000Work;
STATIC_ASSERT_SIZEOF(Actor213000Work, 0x4C4);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// The actor's spawn table: entry 0 is the actor itself, entries 1 to 4 the
/// children its spawn handler creates.
extern TaskDesc D_actor_213000_80157DE0;

/// The actor's message table: `(message id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5 / 0x7DB, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::msgTable`.
extern GpMsgEntry D_actor_213000_80157E1C[];

/// Animation bank table the 0x7D3 handler indexes with the preset's `field_0`.
extern void* D_actor_213000_80157DDC[];

void func_actor_213000_8014A0DC(Task* task);
void func_actor_213000_8014A158(Task* task);
void func_actor_213000_8014A1B8(Task* task);
void func_actor_213000_8014A2C4(Task* task);
void func_actor_213000_8014A5D0(Task* task);
void func_actor_213000_8014A6AC(Task* task);

/// Spawn handler: allocates the work block, seeds its animation bytes and
/// countdown, hides the model, then spawns the four children of the spawn
/// table -- entries 1 and 2 attached to part 8 and parked at `field_4BC` /
/// `field_4C0`, entry 3 attached to part 9 and entry 4 to part 12. Each of the
/// last two has its model's `tpage` / `clut` loaded from the `GpAreaPlace` of
/// the current area selected by the model id the parent's `spawnArg2` carries
/// at `GpEnemy::placeKey >> 12`, and has its texture stream processed twice
/// when it has a buffer. It then publishes the work block's matrices on the
/// model, installs the message table and `Gp_EnemyTaskExit` as the exit
/// callback, and advances to the tick. A failed allocation exits the task
/// instead.
void func_actor_213000_80149E54(Task* task)
{
    Actor213000Work* work;
    TmdObject*       obj;
    GpAreaKey        key;
    Task*            spawned1;
    Task*            spawned2;

    obj  = task->extra;
    work = (Actor213000Work*)memCalloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_478 = 0;
    work->field_477 = -1;
    obj->flags     |= 0x80;
    work->field_4BC = Task_SpawnFromTable(&D_actor_213000_80157DE0, 1, 8, (s32)task);
    work->field_4C0 = Task_SpawnFromTable(&D_actor_213000_80157DE0, 2, 8, (s32)task);
    spawned1        = Task_SpawnFromTable(&D_actor_213000_80157DE0, 3, 9, (s32)task);
    spawned2        = Task_SpawnFromTable(&D_actor_213000_80157DE0, 4, 0xC, (s32)task);
    if (spawned1 != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        GpAreaKey*   sessionKey;
        s32          idx;

        idx        = ((GpEnemy*)task->spawnArg2)->placeKey >> 12;
        model      = (TmdObject*)spawned1->extra;
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    if (spawned2 != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        GpAreaKey*   sessionKey;
        s32          idx;

        model      = (TmdObject*)spawned2->extra;
        idx        = ((GpEnemy*)task->spawnArg2)->placeKey >> 12;
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_213000_8014A6AC(task);
    task->msgTable     = D_actor_213000_80157E1C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// State table of the children spawned from table entries 1 and 2: attach to
/// the parent, idle, kill.
const TaskFuncTable3 D_actor_213000_80149E24 = {
    {
        func_actor_213000_8014A0DC,
        func_actor_213000_8014A158,
        taskKill,
    },
};

/// Body of the children spawned from table entries 1 and 2: dispatches on
/// the state through `D_actor_213000_80149E24`.
void func_actor_213000_8014A084(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E24;
    sp.funcs[task->state](task);
}

/// State 0 of the children spawned from table entries 1 and 2: chains the
/// child's root coordinate under the parent's skeleton part named by the spawn
/// arguments (the parent task and the part index it was spawned with),
/// inherits the parent's light and colour matrices, reparents the task so it
/// runs with the parent, and advances to the idle state.
void func_actor_213000_8014A0DC(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// The idle state of the children spawned from table entries 1 and 2: does
/// nothing.
void func_actor_213000_8014A158(Task* task)
{
}

/// State table of the child spawned from table entry 4: setup, tick, kill.
const TaskFuncTable3 D_actor_213000_80149E30 = {
    {
        func_actor_213000_8014A1B8,
        func_actor_213000_8014A2C4,
        taskKill,
    },
};

/// Body of the child spawned from table entry 4: dispatches on the state
/// through `D_actor_213000_80149E30`.
void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

/// Setup state of the child spawned from table entry 4: hides the child's
/// model, then mirrors the parent's (`spawnArg2`) model flag bits 0x80 and 0x4
/// as the tick state does. It draws the model at order-table offset -2, hangs
/// the child's root coordinate off the parent's part `spawnArg1`, shares the
/// parent's light and colour matrices, reparents the task under the parent and
/// steps to the tick state.
void func_actor_213000_8014A1B8(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Tick state of the child spawned from table entry 4: shows or hides the
/// child's model with the parent's (`spawnArg2`), copying bit 0x80, and
/// copies bit 0x4 too; when the parent's 0x4 is clear the child's buffers are
/// reallocated through `Tmd_AllocBuffers`.
void func_actor_213000_8014A2C4(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

/// Setup state of the child spawned from table entry 3: hangs each of the
/// child's three root coordinates off the parent's part nine slots above it
/// (parts 9 to 11) with a zero local transform, shares the parent's light and
/// colour matrices, mirrors the parent's model bits 0x80 (hidden) and 0x4 as
/// the tick state does, draws the model at order-table offset -4, reparents
/// the task under the parent and steps to the tick state.
void func_actor_213000_8014A35C(Task* task)
{
    Task*       parent;
    TmdObject*  obj;
    TmdObject*  parentObj;
    GpCoordExt* coords;
    GpCoordExt* root;
    s32         i;
    u16         flags;

    parent    = task->spawnArg2;
    obj       = task->extra;
    parentObj = parent->extra;
    for (i = 0; i < 3; i++) {
        coords             = &((GpCoordExt*)((TmdObject*)parent->extra)->coords)[i + 9];
        root               = &((GpCoordExt*)((TmdObject*)task->extra)->coords)[i];
        root->sub          = (GsCOORDINATE2*)coords;
        root->coord.t[0]   = 0;
        root->coord.t[1]   = 0;
        root->coord.t[2]   = 0;
        root->param.rot.vx = 0;
        root->param.rot.vy = 0;
        root->param.rot.vz = 0;
        root->flg          = 0;
    }
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    flags         = obj->flags | 0x80;
    obj->flags    = flags;
    if (!(parentObj->flags & 0x80)) {
        obj->flags = flags & 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -4;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of the child spawned from table entry 3: the same mirroring
/// of the parent's model bits 0x80 (hidden) and 0x4 onto the child's model,
/// reallocating the child's buffers when the parent's 0x4 is clear.
void func_actor_213000_8014A488(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

/// State table of the child spawned from table entry 3: setup, tick, kill.
const TaskFuncTable3 D_actor_213000_80149E3C = {
    {
        func_actor_213000_8014A35C,
        func_actor_213000_8014A488,
        taskKill,
    },
};

/// Body of the child spawned from table entry 3: dispatches on the state
/// through `D_actor_213000_80149E3C`.
void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

/// The actor's three states: spawn, per-frame tick and teardown.
const TaskFuncTable3 D_actor_213000_80149E48 = {
    {
        func_actor_213000_80149E54,
        func_actor_213000_8014A5D0,
        Gp_EnemyTaskExit,
    },
};

/// Body of the actor's task (spawn table entry 0): dispatches on the state
/// through `D_actor_213000_80149E48`.
void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: ticks the work block's animation slots once a preset has
/// started them, and once the view is ready rebuilds model part 1's world
/// matrix and hands its translation to `func_800D7A9C`. The work block's
/// countdown then frees the model's buffers as it reaches zero.
void func_actor_213000_8014A5D0(Task* task)
{
    Actor213000Work* work;
    TmdObject*       extra;
    GsCOORDINATE2*   coords;
    s32              i;

    extra  = (TmdObject*)task->extra;
    work   = (Actor213000Work*)task->work;
    coords = &extra->coords[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        coords->flg = 0;
        Gp_UpdateCoord(coords);
        func_800D7A9C(extra, (VECTOR*)coords->workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_477--;
    }
}

/// Points the model's light and colour matrices at the work block's own pair,
/// then rebuilds model part 1's world matrix and hands its translation to
/// `func_800D7A9C`.
void func_actor_213000_8014A6AC(Task* task)
{
    Actor213000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor213000Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coords          = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Message-0x7D3 handler: a changed bank index re-seeds the whole animation
/// slot array through `func_800B3F84` from the bank table and forgets the
/// current animation id. The preset's id is then stored and installed on every
/// slot - through `func_800B4114` with a blend length of 6 when the preset's
/// `field_8` is set, through `Gp_AnimResetSlot` otherwise - after which every
/// slot is ticked once and `field_474` latches. Returns 0.
s32 func_actor_213000_8014A70C(Task* task, s32 arg1, GpAnimArg* msg)
{
    Actor213000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213000Work*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->field_476) {
        work->field_476 = msg->animBlock.index;
        work->field_475 = -1;
        func_800B3F84(&work->rig.anim, D_actor_213000_80157DDC[work->field_476], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->field_475, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->field_474 = 1;
    return 0;
}

/// Message-0x7D4 handler: places the actor at the message's arguments - the
/// translation goes straight into the root coordinate's local matrix, the
/// Euler angles into the coordinate's `rot` slot, from which the rotation is
/// rebuilt. Clearing `flg` has the world matrix recomputed. Returns 0.
s32 func_actor_213000_8014A828(Task* task, s32 arg1, GpXformArg* args)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Message-0x7D5 display handler, switching on the message's mode word. Mode
/// 0 hides the model and clears flag 0x4; 1 shows it, reallocates its buffers
/// through `Tmd_AllocBuffers` and clears 0x4; 2 hides it, sets 0x4 and starts
/// the work block's countdown at 2, after which the tick frees the buffers; 3
/// shows it and sets 0x4. The handled modes return 0; any other mode changes
/// nothing and returns 1.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block.
s32 func_actor_213000_8014A8A4(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor213000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor213000Work*)task->work;
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
            work->field_477 = mode;
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

/// Message-0x7DB handler: shows or hides the models of the two children the
/// work block parks at `field_4BC` / `field_4C0`. Mode 0 shows the first
/// (clears bit 0x80 of its `TmdObject::flags`) and 1 hides it; 2 and 3 show
/// and hide the second. A missing child or an unknown mode touches nothing.
/// Every path returns 0.
/// Both `|= 0x80` arms are written out in the source; the post-reload `jump2`
/// cross-jump folds mode 1's copy into mode 3's, which is why retail's mode-1
/// arm is only the `lw` plus a jump while modes 0 and 2 each keep their own
/// `& 0xFF7F` copy. Which tails jump2 merges is decided by which jumps share a
/// target label, not by how alike the bodies are.
s32 func_actor_213000_8014A980(Task* task, s32 arg1, GpCmdArg* msg)
{
    Actor213000Work* work;
    Task*            child;
    u16              mode;

    mode = msg->command;
    work = (Actor213000Work*)task->work;

    switch (mode) {
        case 0:
            child = work->field_4BC;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags &= 0xFF7F;
            }
            break;
        case 1:
            child = work->field_4BC;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags |= 0x80;
            }
            break;
        case 2:
            child = work->field_4C0;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags &= 0xFF7F;
            }
            break;
        case 3:
            child = work->field_4C0;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags |= 0x80;
            }
            break;
    }
    return 0;
}
