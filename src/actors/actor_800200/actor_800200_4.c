#include "common.h"

#include "main/session.h"
#include "main/task.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

#include "actors/actor_800200.h"

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `Gp_ActorSlots` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
void func_actor_800200_801626A0(Task* task)
{
    GameActor* actor;

    actor            = (GameActor*)task->work;
    Gp_ActorSlots[1] = NULL;
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    taskKill(task);
}
