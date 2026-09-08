#include "common.h"

#include "actors/actor_104000.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern TaskFunc D_actor_104000_8013E50C[];

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138698);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801387B4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_8013887C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801388E4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_801389E8);

/// Restart handler: when the work block's field_4 latch is set, clears the
/// caller's flag and the display object, rearms the motion state (6/1/0) and
/// flips the gate bits on four records of the 0x38-stride flag table before
/// running the shared per-frame step. Otherwise it runs that step first and
/// only advances the work state when bit 0 of field_58 is set.
void func_actor_104000_80138AA0(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 6;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_36E |= 0x8000;
        work->field_3A6 &= 0x7FFF;
        work->field_3DE &= 0x7FFF;
        work->field_28E |= 0x4000;
        func_actor_104000_80132C8C(arg1);
    } else {
        func_actor_104000_80132C8C(arg1);
        if (work->field_58 & 1) {
            work->field_0 = 7;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138B58);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138C6C);

/// Resets the overlay's counter tables, then asks the slot-4 task to forward
/// message 0x7DB with the `{ 3, 0x10, 1 }` record before advancing a state.
void func_actor_104000_80138CC8(Task* arg0)
{
    Actor104000Msg7DA msg;
    s16               i;

    for (i = 0; i < 6; i++) {
        D_actor_104000_8013E538[i] = 0;
    }
    D_actor_104000_8013E530[1] = 0;
    D_actor_104000_8013E530[0] = 0;
    msg.field_0                = 3;
    msg.field_1                = 0x10;
    msg.field_2                = 1;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138D74);

/// Per-frame entry point: runs the actor task's current state from the
/// dispatch table in the overlay's trailing data.
void func_actor_104000_80138E5C(Task* arg0)
{
    D_actor_104000_8013E50C[arg0->state](arg0);
}
