#include "common.h"

#include <psyq/abs.h>

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame task that turns the player (`Game_GetPtrSlot(3)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the translation of the *second*
/// `GsCOORDINATE2` node of the target's model (`field_8[1]`) minus the
/// player's own (`field_8[0]`); the delta against `GameActor::field_52` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `Task_Kill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_dryfield_main_street_8017E1C0(Task* task)
{
    Task*          player;
    GameActor*     actor;
    GpWorkObj*     work;
    GsCOORDINATE2* self;
    GsCOORDINATE2* target;
    s32            angle;
    s32            delta;
    s32            magnitude;
    s32            step;
    s32            wrapped;

    player = Game_GetPtrSlot(3);
    actor  = (GameActor*)player->work;
    work   = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
    if ((work != NULL) && (gGameSession->eventState != 0)) {
        self      = ((TmdObject*)player->extra)->coords;
        target    = &((TmdObject*)((Task*)work->field_0)->extra)->coords[1];
        angle     = ratan2(target->coord.t[0] - self->coord.t[0], target->coord.t[2] - self->coord.t[2]);
        delta     = angle - actor->field_52;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->field_52 = (s16)((u16)actor->field_52 + step);
            return;
        }
        actor->field_52 = angle;
    }
    Task_Kill(task);
}

void func_dryfield_main_street_8017E2F4(s32 arg0)
{
    Gp_ArmStateF0(arg0);
    Gp_SetItemSeenBit(0x10A, 1);
}
