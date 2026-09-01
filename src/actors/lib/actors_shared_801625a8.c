#include "common.h"

#include "actors/actors_shared_801625a8.h"

#include "main/session.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Task exit callback: drops the actor out of the second `Gp_ActorSlots` slot,
/// takes its two display nodes back off their lists, and kills the task.
void ActorsShared801625a8(Task* task)
{
    GameActor* actor;

    actor                             = (GameActor*)task->idMap;
    ((GpActorWork**)Gp_ActorSlots)[1] = NULL;
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Task_Kill(task);
}
