#include "common.h"

#include "actors/actors_shared_801511c8.h"

/// Task exit callback: takes the task's display node back off the object list
/// and kills the task.
void ActorsShared801511c8(Task* arg0)
{
    Gp_UnlinkObj(&((ActorShared801511c8Work*)arg0->idMap)->obj);
    Task_Kill(arg0);
}
