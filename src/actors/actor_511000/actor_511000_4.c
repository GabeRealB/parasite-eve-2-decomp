#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern SVECTOR    D_actor_511000_80147344[];
extern SVECTOR    D_actor_511000_80147704[];
extern SVECTOR    D_actor_511000_80147AC4;
extern u16*       D_actor_511000_80147EB0;
extern GpMsgEntry D_actor_511000_80148FC4[];

/// The three texture records the message-0x7E0 handler uploads, one per mode.
/// Each is a lone `GpImgRec` whose 0x18x0x10 source rect repeats the size the
/// handler's scratch `RECT` carries and whose `data` points at its pixel blob.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_801327A0);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_8013287C);

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

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_801330F0);

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

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_4", func_actor_511000_801332E4);

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
