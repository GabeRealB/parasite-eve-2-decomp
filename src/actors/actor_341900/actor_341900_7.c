#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

#include "actors/actor_341900.h"

/// Script callback: sends message 0x7DA to the slot-4 task, tagged with the
/// current session's two id bytes and the script's selector, asking for the
/// 0x7DB reply.
void func_actor_341900_80163334(s16 arg0)
{
    Actor341900Msg7DA msg;

    msg.field_0 = gGameSession->at4.loc.stage;
    msg.field_1 = gGameSession->at4.loc.area;
    msg.field_2 = arg0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}
