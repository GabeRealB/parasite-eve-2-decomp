#include "common.h"

#include "actors/actor_100400.h"
#include "main/task.h"

#include "gameplay/3A34.h"

/// Last kill-path state of the marker task. Each frame it clears the marker
/// coordinate's `flg` and counts `killCountdown` up; on the twelfth frame it
/// unlinks the marker's display object and kills the task.
void Actor00400_Fn0A28C(Task* task)
{
    Actor100400MarkerWork* work;
    Actor100400Ctx*        ctx;
    u16                    countdown;

    work                = (Actor100400MarkerWork*)task->work;
    ctx                 = (Actor100400Ctx*)task->extra;
    ctx->field_8->flg   = 0;
    countdown           = task->killCountdown + 1;
    task->killCountdown = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        taskKill(task);
    }
}
