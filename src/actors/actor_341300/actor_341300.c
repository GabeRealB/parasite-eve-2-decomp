#include "common.h"

#include <psyq/abs.h>

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// Placement record the overlay's data table points at, read here only as the
/// target position's x/z pair.
extern VECTOR D_actor_341300_80165330;

INCLUDE_RODATA("actors/nonmatchings/actor_341300/actor_341300", D_actor_341300_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300", func_actor_341300_80161E84);

/// Per-frame task that turns the player (`Game_GetPtrSlot(3)`, whose
/// `Task::idMap` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the target's x/z pair minus the player's own
/// coordinate translation; the delta against `GameActor::field_52` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `Task_Kill` target, reached both
/// when the work lookup or `Game_Session::field_1` fails and on the frame the
/// facing settles.
void func_actor_341300_80162278(Task* task)
{
    Task*          player;
    GameActor*     actor;
    GpWorkObj*     work;
    GsCOORDINATE2* self;
    VECTOR*        target;
    s32            angle;
    s32            delta;
    s32            magnitude;
    s32            step;
    s32            wrapped;

    player = Game_GetPtrSlot(3);
    actor  = (GameActor*)player->idMap;
    work   = Gp_FindWorkById(Game_Session->field_6 | (Game_Session->field_7 << 8));
    if ((work != NULL) && (Game_Session->field_1 != 0)) {
        self      = ((TmdObject*)player->extra)->field_8;
        target    = &D_actor_341300_80165330;
        angle     = ratan2(target->vx - self->coord.t[0], target->vz - self->coord.t[2]);
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
