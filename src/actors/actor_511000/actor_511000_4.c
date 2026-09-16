#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern SVECTOR    D_actor_511000_80147344[];
extern SVECTOR    D_actor_511000_80147704[];
extern SVECTOR    D_actor_511000_80147AC4;
extern u16*       D_actor_511000_80147EB0;
extern GpMsgEntry D_actor_511000_80148FC4[];

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `GpViewRec` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `Gp_TrySpawnViewTask`, exactly as
/// `Gp_SpawnViewTasks` hands its own stage record.
extern GpViewRec D_actor_511000_80147EE4[];

/// The three texture records the message-0x7E0 handler uploads, one per mode.
/// Each is a lone `GpImgRec` whose 0x18x0x10 source rect repeats the size the
/// handler's scratch `RECT` carries and whose `data` points at its pixel blob.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. Mode 0 shows
/// the model (`field_C` bit 0x80) and clears the 4 flag, 1 hides it, frees the
/// aux buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_480`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The handler reads `idMap` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_141000_80133E8C` / `func_actor_503500_80132584`.
s32 func_actor_511000_801327A0(GpActorWork* arg0, s32 arg1, s32 mode)
{
    TmdObject*        obj;
    Actor511000Work2* work;
    s32               ret;

    obj  = arg0->extra;
    work = (Actor511000Work2*)arg0->actor;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C        |= 0x80;
            work->field_480.word = mode;
            obj->field_C        |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: un-hides the model its first child task carries in
/// `Task::extra` (`field_C` bit 0x80) for mode 1 and hides it for mode 0, then
/// hides the second child as well on the mode-1 path -- the same two tasks
/// `func_actor_511000_80132480` parked at `field_4C4` / `field_4C8`. Any other
/// mode leaves both alone.
/// The `default:` arm jumps straight to the shared `return 0` instead of
/// falling through the hide block: retail's single epilogue is only reached
/// that way, the hide block and the shared return merging into one block whose
/// first label sits on the value store.
s32 func_actor_511000_8013287C(GpActorWork* arg0, s32 arg1, Actor511000Msg* msg)
{
    Actor511000Work2* work;
    Task*             child;
    u16               mode;

    mode = msg->field_2;
    work = (Actor511000Work2*)arg0->actor;

    switch (mode) {
        case 0:
            child = work->field_4C4;
            break;
        case 1:
            child = work->field_4C4;
            if (child != NULL) {
                ((TmdObject*)child->extra)->field_C |= 0x80;
            }
            child = work->field_4C8;
            break;
        default:
            goto out;
    }

    if (child != NULL) {
        ((TmdObject*)child->extra)->field_C &= 0xFF7F;
    }
out:
    return 0;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records
/// over the 0x18x0x10 rect at y 0x28 -- `D_actor_511000_801472B4` for mode 1,
/// `D_actor_511000_80146C74` for modes 0 and 2, and `D_actor_511000_80146F94`
/// for mode 3, which sets the work block's `field_4D0` / `field_4CC` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_511000_80132904(GpActorWork* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_511000_801472B4;
            break;
        case 0:
        case 2:
            img = &D_actor_511000_80146C74;
            break;
        case 3:
            ((Actor511000Work2*)arg0->actor)->field_4D0 = 1;
            ((Actor511000Work2*)arg0->actor)->field_4CC = 1;
            img                                         = &D_actor_511000_80146F94;
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_801329C4);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_80132E6C);

/// Spawn/setup state: allocates the 0x70 work block, parks it in `idMap`,
/// arms the buffer-free countdown at -1, un-hides the model (`field_C` bit
/// 0x80), places it at rot/trans index 0, binds light/color, installs the
/// message table, and publishes `work->field_C` through
/// `D_actor_511000_80147EB0` before advancing to the per-frame state.
void func_actor_511000_80133034(Task* task)
{
    Actor511000Work* work;
    TmdObject*       extra;

    extra = (TmdObject*)task->extra;
    work  = (Actor511000Work*)Mem_Calloc(0x70, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_8   = -1;
    extra->field_C |= 0x80;
    func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
    func_actor_511000_801337F0(task);
    do {
        task->field_24          = D_actor_511000_80148FC4;
        D_actor_511000_80147EB0 = &work->field_C;
    } while (0);
    task->state += 1;
}

/// Per-frame state: while the model is hidden (`field_C` bit 0x80 clear) it
/// refreshes the root coordinate, rebuilds the colour matrix from that
/// coordinate's own translation, and runs the work block's follow-up. Once the
/// session reaches mode 0x18 it walks `killCountdown` up to 0x77, spawning a
/// view task for the camera record at each index and re-posing the model from
/// the matching rotations, and finally runs the `Tmd_FreeBuffers` countdown the
/// spawn state armed at -1, freeing the buffers and latching the field back to
/// -1 on the frame the countdown reaches zero.
void func_actor_511000_801330F0(Task* task)
{
    Actor511000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              countdown;
    s16              frame;

    obj   = (TmdObject*)task->extra;
    work  = (Actor511000Work*)task->idMap;
    coord = obj->field_8;

    if (!(obj->field_C & 0x80)) {
        Gp_UpdateCoord(coord);
        func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
        func_actor_511000_80132E6C((Actor511000Work*)task->idMap);
    }
    if (Game_Session->field_4 == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        Gp_TrySpawnViewTask((s32)&D_actor_511000_80147EE4[task->killCountdown]);
        func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
        coord->flg = 0;
    }
    countdown = work->field_8;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(obj);
            countdown = work->field_8;
        }
        work->field_8 = countdown - 1;
    }
}

void func_actor_511000_80133220(Task* task)
{
    Task_Kill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, and reparents the task.
void func_actor_511000_80133240(Task* task)
{
    Task*          parent;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->field_8;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    extra->field_C  = 0x80;
    coord           = extra->field_8;
    if (!(parentExtra->field_C & 0x80)) {
        extra->field_C = 0;
    }
    func_actor_511000_80133760(task);
    coord->sub = dest;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tracks the parent model's visibility bit every frame: while the parent model
/// is hidden (`field_C` bit 0x80 clear) this clears its own bit and, for
/// spawnArg1 1 or 2, spins the root coordinate's yaw (0x46) by 0x294 or its
/// pitch (0x44) by 0x3E8, wrapping each to 0x1000. The rotation matrix is then
/// rebuilt from the angles and the coordinate's `flg` cleared. With the parent
/// visible the rotation is left alone and the visibility bit is set instead.
void func_actor_511000_801332E4(Task* task)
{
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;

    extra       = (TmdObject*)task->extra;
    coord       = (Actor511000Coord*)extra->field_8;
    parentExtra = (TmdObject*)((Task*)task->spawnArg2)->extra;

    if (!(parentExtra->field_C & 0x80)) {
        extra->field_C &= 0xFF7F;

        switch (task->spawnArg1) {
            case 1:
                coord->rot.vy = ((u16)coord->rot.vy + 0x294) & 0xFFF;
                break;
            case 2:
                coord->rot.vx = ((u16)coord->rot.vx + 0x3E8) & 0xFFF;
                break;
        }

        RotMatrix(&coord->rot, &coord->coord);
        coord->flg = 0;
        return;
    }
    extra->field_C |= 0x80;
}

void func_actor_511000_801333A4(Task* task)
{
    Task_Kill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, copies `D_actor_511000_80147AC4` onto the Euler angles,
/// rebuilds the rotation matrix, and reparents the task.
void func_actor_511000_801333C4(Task* task)
{
    Task*             parent;
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;
    GsCOORDINATE2*    dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->field_8;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    extra->field_C  = 0x80;
    coord           = (Actor511000Coord*)extra->field_8;
    if (!(parentExtra->field_C & 0x80)) {
        extra->field_C = 0;
    }
    func_actor_511000_80133760(task);
    ((GsCOORDINATE2*)coord)->sub = dest;
    Task_Reparent(parent, task);
    coord->rot.vx = D_actor_511000_80147AC4.vx;
    coord->rot.vy = D_actor_511000_80147AC4.vy;
    coord->rot.vz = D_actor_511000_80147AC4.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg   = 0;
    task->state += 1;
}

void func_actor_511000_80133498(Task* task)
{
    Task_Kill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}
