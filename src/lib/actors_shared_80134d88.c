#include "common.h"

#include "actors/actors_shared_80134d88.h"

#include "rooms/room_common.h"

void ActorsShared80134d88(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
