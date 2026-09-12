#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/sound.h"

/// The overlay's event/controller task, whose `idMap` holds an
/// `Actor444000EventWork`.
extern Task* D_actor_444000_80161860;

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000", func_actor_444000_80132054);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000", func_actor_444000_801321FC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000", func_actor_444000_80132358);

/// Play the event's sound cue once, latching a flag so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    if (work->field_2A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_2A = 1;
    }
}

/// Forward a message to the slot-3 task the event work block carries.
void func_actor_444000_8013265C(s32 arg0)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    Gp_DispatchMsg(work->field_20, 0x3F3, arg0, 0);
}

/// Kill the subordinate task the event work block carries, if it is still alive.
void func_actor_444000_80132694(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    if (work->field_24 != NULL) {
        Task_Kill(work->field_24);
        work->field_24 = NULL;
    }
}

void func_actor_444000_801326DC(void)
{
    Actor444000Msg7DA msg;

    msg.field_0 = 0;
    msg.field_1 = 0x2C;
    msg.field_2 = 3;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000", D_actor_444000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000", jtbl_actor_444000_80131E24);
