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

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_3", func_actor_104000_80138AA0);

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
