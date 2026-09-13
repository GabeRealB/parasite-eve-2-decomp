#include "common.h"

#include "actors/actor_100400.h"
#include "gameplay/3A34.h"

void Actor00400_Fn0A28C(Task* task)
{
    Actor100400Work* work;
    Actor100400Ctx*  ctx;
    u16              countdown;

    work                = (Actor100400Work*)task->idMap;
    ctx                 = (Actor100400Ctx*)task->extra;
    ctx->field_8->flg   = 0;
    countdown           = task->killCountdown + 1;
    task->killCountdown = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj((GpObj*)work->field_8);
        Task_Kill(task);
    }
}
