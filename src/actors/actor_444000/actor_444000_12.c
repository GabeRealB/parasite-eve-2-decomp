#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

/// Send message 0x7DA to the slot-4 task, tagged with the current session's
/// stage and area and the caller's selector. Nothing in the actor calls it.
void func_actor_444000_80132724(s16 arg0)
{
    Actor444000Msg7DA msg;

    msg.field_0 = gGameSession->at4.loc.stage;
    msg.field_1 = gGameSession->at4.loc.area;
    msg.field_2 = arg0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}
