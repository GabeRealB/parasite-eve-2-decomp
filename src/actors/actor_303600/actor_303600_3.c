#include "common.h"

#include "actors/actor_303600.h"
#include "main/mem.h"
#include "main/tmd.h"

/// Message 0x7DB handler, listed in `D_actor_303600_8016E480` -- the table
/// `func_actor_303600_801626C0` installs at `Task::msgTable`.  The payload is
/// the 0x7DA record `Gp_SendMsgType9` forwards back to the slot-4 task's
/// type-9 children, so the halfword switched on here is the sender's selector:
/// 0 arms the rig's speed at 384.0 (16.16) with a positive ramp, 1 with a
/// negative one (-6.0 toward -48.0), and every other selector exits the task
/// through its own `Task::exitCallback`.  `func_actor_303600_801627B8` is what
/// consumes the ramped speed.
s32 func_actor_303600_80162870(Task* task, s32 msgId, Actor303600Msg7DA* msg)
{
    Actor303600RigWork* work;

    work = (Actor303600RigWork*)task->work;
    switch (msg->field_2) {
        case 0:
            work->field_28 = 0x01800000;
            work->field_34 = 0x00080000;
            work->field_38 = 0x03000000;
            break;
        case 1:
            work->field_34 = 0xFFFA0000;
            work->field_38 = 0xFD000000;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_303600/actor_303600_3", D_actor_303600_80161E48);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_801628E4);

/// Builds the actor's light / colour matrix pair, hangs it off the task's
/// `work` slot, and splices this task's model root under its spawn parent's.
void func_actor_303600_80162950(Task* task)
{
    Task*                 parent      = task->spawnArg2;
    TmdObject*            obj         = task->extra;
    GsCOORDINATE2*        coord       = obj->field_8;
    TmdObject*            parentObj   = parent->extra;
    GsCOORDINATE2*        parentCoord = parentObj->field_8;
    Actor303600LightMats* mats;

    mats = Mem_Calloc(0x44, 0);
    if (mats == NULL) {
        Task_Kill(task);
        return;
    }

    task->work = (TaskIdMap*)mats;
    coord->sub = parentCoord;
    coord->flg = 0;
    func_actor_303600_80162A0C(task);
    Task_Reparent(parent, task);
    obj->field_C &= 0xFF7F;
    task->state  += 1;
}

void func_actor_303600_80162A04(void)
{
}

/// Points the task's model at the light / colour matrix pair in its own work
/// block and loads the overlay's three flat lights into them.
void func_actor_303600_80162A0C(Task* task)
{
    Actor303600LightMats* mats = (Actor303600LightMats*)task->work;
    TmdObject*            obj  = task->extra;
    GsF_LIGHT*            light;
    s32                   i;

    obj->field_1C = &mats->lightMtx;
    obj->field_20 = &mats->colorMtx;
    for (i = 0, light = D_actor_303600_8016E490; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &mats->lightMtx, &mats->colorMtx);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_3", func_actor_303600_80162A7C);
