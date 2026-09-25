#include "common.h"

#include "actors/actor_213000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E30;
extern TaskFuncTable3 D_actor_213000_80149E3C;
extern TaskFuncTable3 D_actor_213000_80149E48;

void func_actor_213000_8014A158(void)
{
}

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

void func_actor_213000_8014A35C(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    RoomCoord* coords;
    RoomCoord* root;
    s32        i;
    u16        flags;

    parent    = task->spawnArg2;
    obj       = task->extra;
    parentObj = parent->extra;
    /* Each of the three roots hangs off the spawner's part nine slots above it */
    for (i = 0; i < 3; i++) {
        coords           = &((RoomCoord*)((TmdObject*)parent->extra)->coords)[i + 9];
        root             = &((RoomCoord*)((TmdObject*)task->extra)->coords)[i];
        root->sub        = (GsCOORDINATE2*)coords;
        root->coord.t[0] = 0;
        root->coord.t[1] = 0;
        root->coord.t[2] = 0;
        root->rot.vx     = 0;
        root->rot.vy     = 0;
        root->rot.vz     = 0;
        root->flg        = 0;
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

void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: advances the animation context's slots while the work block
/// says it is live, and once the session has finished loading a stage rebuilds
/// model part 1's world matrix and hands its translation to the ground-shadow
/// helper. The work block's countdown then frees the model buffers as it
/// reaches zero.
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
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
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

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
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

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern void* D_actor_213000_80157DDC[];

/// Animation-preset handler: while the preset's bank index differs from the
/// one the work block latched, clears the animation id to -1, latches the new
/// index and re-seeds the animation context from that bank
/// (`D_actor_213000_80157DDC`). The preset's `field_4` then replaces the id,
/// and `field_8` picks the slot walk -- the blended `func_800B4114` reseed with
/// a literal count of 6, or the plain `Gp_AnimResetSlot` -- before every slot
/// is ticked once and the `field_474` latch is set. The same body as
/// `func_actor_335800_801632A4` and `func_actor_361100_801634D0`.
s32 func_actor_213000_8014A70C(Task* task, s32 arg1, Actor213000AnimPreset* msg)
{
    Actor213000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        work->field_475 = -1;
        func_800B3F84(&work->anim, D_actor_213000_80157DDC[work->field_476], ext, work->field_334,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}
