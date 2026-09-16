#include "common.h"
#include "main/fs.h"
#include "main/session.h"

#include "gameplay/D4.h"

#include "actors/actor_120300.h"

/// Broadcasts message 0x7D5 -- the visibility control the actor's display task
/// handles -- to the actor itself and to the two task slots on its work block.
/// Sending it is the whole body: `arg0` is the message's payload and only 0/1
/// are accepted.
void func_actor_120300_80133D04(s32 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->field_1C;

    if (arg0 == 0) {
        Gp_DispatchMsg((Task*)D_actor_120300_80141BA8, 0x7D5, 0, 0);
        Gp_DispatchMsg(work->field_4B8, 0x7D5, 0, 0);
        Gp_DispatchMsg(work->field_4BC, 0x7D5, 0, 0);
    } else if (arg0 == 1) {
        Gp_DispatchMsg((Task*)D_actor_120300_80141BA8, 0x7D5, 1, 0);
        Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
        Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
    }
}

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    Game_Session->field_52 = 1;
}
