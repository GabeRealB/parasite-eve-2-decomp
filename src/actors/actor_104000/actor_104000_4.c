#include "common.h"

#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern TaskFunc D_actor_104000_8013E50C[];

/// Fade-out handler: when the work block's field_4 latch is set, clears the
/// caller's flag and the display object, rearms the motion state (0x10/0) and
/// restarts the frame counter. Otherwise it steps every frame, kicking a pad
/// lerp and a 0x3F9 broadcast on every eighth frame, and after 0x28 frames
/// arms the timeout and asks the slot-3 task which state to move to.
void func_actor_104000_80138698(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;
    Task*             task;
    s16               counter;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_176     = 0x10;
        work->field_178     = 0;
        work->obj270.flags |= 0x4000;
        func_actor_104000_80132C8C(arg1);
        work->field_6 = 0;
        return;
    }
    func_actor_104000_80132C8C(arg1);
    if ((work->field_6 & 7) == 0) {
        Gp_SpawnPadLerp(3, 0xFF, 8);
        task = Game_GetPtrSlot(3);
        Gp_DispatchMsg(task, 0x3F9, Gp_PackObjPair((GpObj50*)arg0, 0), 0);
    }
    counter       = work->field_6 + 1;
    work->field_6 = counter;
    if (counter >= 0x29) {
        work->field_490 = 0x270F;
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)work->field_47C, 0) == 0) {
            work->field_0 = 5;
        } else {
            work->field_0 = 0xD;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_4", func_actor_104000_801387B4);
